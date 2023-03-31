// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2022 flowln <flowlnlnln@gmail.com>
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *  Copyright (C) 2023 TheKodeToad <TheKodeToad@proton.me>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include "Download.h"
#include <QRegularExpression>
#include <QUrl>

#include <QDateTime>
#include <QFileInfo>

#include "ByteArraySink.h"
#include "ChecksumValidator.h"
#include "FileSystem.h"
#include "MetaCacheSink.h"

#include "BuildConfig.h"
#include "Application.h"

Q_LOGGING_CATEGORY(DownloadLogC, "Task.Net.Download")

namespace Net {

QString truncateUrlHumanFriendly(QUrl &url, int max_len, bool hard_limit = false)
{   
    auto display_options = QUrl::RemoveUserInfo | QUrl::RemoveFragment | QUrl::NormalizePathSegments;
    auto str_url = url.toDisplayString(display_options);
    if (str_url.length() <= max_len)
        return str_url;

    QRegularExpression re(R"(^([\w]+:\/\/)([\w._-]+\/)([\w._-]+\/).*(\/[^]+[^]+)$)");
    
    auto url_compact = QString(str_url);
    url_compact.replace(re, "\\1\\2\\3...\\4");
    if (url_compact.length() >= max_len) {
        auto url_compact = QString(str_url);
        url_compact.replace(re, "\\1\\2...\\4");
    }


    if ((url_compact.length() >= max_len) && hard_limit) {
        auto to_remove = url_compact.length() - max_len + 3;
        url_compact.remove(url_compact.length() - to_remove - 1, to_remove);
        url_compact.append("...");
    }

    return url_compact;

}

auto Download::makeCached(QUrl url, MetaEntryPtr entry, Options options) -> Download::Ptr
{
    auto dl = makeShared<Download>();
    dl->m_url = url;
    dl->setObjectName(QString("CACHE:") + url.toString());
    dl->m_options = options;
    auto md5Node = new ChecksumValidator(QCryptographicHash::Md5);
    auto cachedNode = new MetaCacheSink(entry, md5Node, options.testFlag(Option::MakeEternal));
    dl->m_sink.reset(cachedNode);
    return dl;
}

auto Download::makeByteArray(QUrl url, QByteArray* output, Options options) -> Download::Ptr
{
    auto dl = makeShared<Download>();
    dl->m_url = url;
    dl->setObjectName(QString("BYTES:") + url.toString());
    dl->m_options = options;
    dl->m_sink.reset(new ByteArraySink(output));
    return dl;
}

auto Download::makeFile(QUrl url, QString path, Options options) -> Download::Ptr
{
    auto dl = makeShared<Download>();
    dl->m_url = url;
    dl->setObjectName(QString("FILE:") + url.toString());
    dl->m_options = options;
    dl->m_sink.reset(new FileSink(path));
    return dl;
}

void Download::addValidator(Validator* v)
{
    m_sink->addValidator(v);
}

void Download::executeTask()
{
    setStatus(tr("Downloading %1").arg(truncateUrlHumanFriendly(m_url, 60)));

    if (getState() == Task::State::AbortedByUser) {
        qCWarning(DownloadLogC) << getUid().toString() << "Attempt to start an aborted Download:" << m_url.toString();
        emitAborted();
        return;
    }

    QNetworkRequest request(m_url);
    m_state = m_sink->init(request);
    switch (m_state) {
        case State::Succeeded:
            emit succeeded();
            qCDebug(DownloadLogC) << getUid().toString() << "Download cache hit " << m_url.toString();
            return;
        case State::Running:
            qCDebug(DownloadLogC) << getUid().toString() << "Downloading " << m_url.toString();
            break;
        case State::Inactive:
        case State::Failed:
            emitFailed();
            return;
        case State::AbortedByUser:
            emitAborted();
            return;
    }

    request.setHeader(QNetworkRequest::UserAgentHeader, APPLICATION->getUserAgent().toUtf8());
    // TODO remove duplication
    if (APPLICATION->capabilities() & Application::SupportsFlame && request.url().host() == QUrl(BuildConfig.FLAME_BASE_URL).host()) {
        request.setRawHeader("x-api-key", APPLICATION->getFlameAPIKey().toUtf8());
    } else if (request.url().host() == QUrl(BuildConfig.MODRINTH_PROD_URL).host() ||
               request.url().host() == QUrl(BuildConfig.MODRINTH_STAGING_URL).host()) {
        QString token = APPLICATION->getModrinthAPIToken();
        if (!token.isNull())
            request.setRawHeader("Authorization", token.toUtf8());
    }
    
    m_last_progress_time = m_clock.now();
    m_last_progress_bytes = 0;

    QNetworkReply* rep = m_network->get(request);
    
    m_reply.reset(rep);
    connect(rep, &QNetworkReply::downloadProgress, this, &Download::downloadProgress);
    connect(rep, &QNetworkReply::finished, this, &Download::downloadFinished);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    connect(rep, SIGNAL(errorOccurred(QNetworkReply::NetworkError)), SLOT(downloadError(QNetworkReply::NetworkError)));
#else
    connect(rep, SIGNAL(error(QNetworkReply::NetworkError)), SLOT(downloadError(QNetworkReply::NetworkError)));
#endif
    connect(rep, &QNetworkReply::sslErrors, this, &Download::sslErrors);
    connect(rep, &QNetworkReply::readyRead, this, &Download::downloadReadyRead);
}

void Download::downloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    auto now = m_clock.now();
    auto elapsed = now - m_last_progress_time;
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    auto bytes_recived_since = bytesReceived - m_last_progress_bytes;
    if (elapsed_ms > 0) {
        m_details = humanReadableFileSize(bytes_recived_since / elapsed_ms * 1000) + "/s";
    } else {
        m_details = "0 b/s";
    }  

    setProgress(bytesReceived, bytesTotal);
}

void Download::downloadError(QNetworkReply::NetworkError error)
{
    if (error == QNetworkReply::OperationCanceledError) {
        qCCritical(DownloadLogC) << getUid().toString() << "Aborted " << m_url.toString();
        m_state = State::AbortedByUser;
    } else {
        if (m_options & Option::AcceptLocalFiles) {
            if (m_sink->hasLocalData()) {
                m_state = State::Succeeded;
                return;
            }
        }
        // error happened during download.
        qCCritical(DownloadLogC) << getUid().toString() << "Failed " << m_url.toString() << " with reason " << error;
        m_state = State::Failed;
    }
}

void Download::sslErrors(const QList<QSslError>& errors)
{
    int i = 1;
    for (auto error : errors) {
        qCCritical(DownloadLogC) << getUid().toString() << "Download" << m_url.toString() << "SSL Error #" << i << " : " << error.errorString();
        auto cert = error.certificate();
        qCCritical(DownloadLogC) << getUid().toString() << "Certificate in question:\n" << cert.toText();
        i++;
    }
}

auto Download::handleRedirect() -> bool
{
    QUrl redirect = m_reply->header(QNetworkRequest::LocationHeader).toUrl();
    if (!redirect.isValid()) {
        if (!m_reply->hasRawHeader("Location")) {
            // no redirect -> it's fine to continue
            return false;
        }
        // there is a Location header, but it's not correct. we need to apply some workarounds...
        QByteArray redirectBA = m_reply->rawHeader("Location");
        if (redirectBA.size() == 0) {
            // empty, yet present redirect header? WTF?
            return false;
        }
        QString redirectStr = QString::fromUtf8(redirectBA);

        if (redirectStr.startsWith("//")) {
            /*
             * IF the URL begins with //, we need to insert the URL scheme.
             * See: https://bugreports.qt.io/browse/QTBUG-41061
             * See: http://tools.ietf.org/html/rfc3986#section-4.2
             */
            redirectStr = m_reply->url().scheme() + ":" + redirectStr;
        } else if (redirectStr.startsWith("/")) {
            /*
             * IF the URL begins with /, we need to process it as a relative URL
             */
            auto url = m_reply->url();
            url.setPath(redirectStr, QUrl::TolerantMode);
            redirectStr = url.toString();
        }

        /*
         * Next, make sure the URL is parsed in tolerant mode. Qt doesn't parse the location header in tolerant mode, which causes issues.
         * FIXME: report Qt bug for this
         */
        redirect = QUrl(redirectStr, QUrl::TolerantMode);
        if (!redirect.isValid()) {
            qCWarning(DownloadLogC) << getUid().toString() << "Failed to parse redirect URL:" << redirectStr;
            downloadError(QNetworkReply::ProtocolFailure);
            return false;
        }
        qCDebug(DownloadLogC) << getUid().toString() << "Fixed location header:" << redirect;
    } else {
        qCDebug(DownloadLogC) << getUid().toString() << "Location header:" << redirect;
    }

    m_url = QUrl(redirect.toString());
    qCDebug(DownloadLogC) << getUid().toString() << "Following redirect to " << m_url.toString();
    startAction(m_network);

    return true;
}

void Download::downloadFinished()
{
    // handle HTTP redirection first
    if (handleRedirect()) {
        qCDebug(DownloadLogC) << getUid().toString() << "Download redirected:" << m_url.toString();
        return;
    }

    // if the download failed before this point ...
    if (m_state == State::Succeeded)  // pretend to succeed so we continue processing :)
    {
        qCDebug(DownloadLogC) << getUid().toString() << "Download failed but we are allowed to proceed:" << m_url.toString();
        m_sink->abort();
        m_reply.reset();
        emit succeeded();
        return;
    } else if (m_state == State::Failed) {
        qCDebug(DownloadLogC) << getUid().toString() << "Download failed in previous step:" << m_url.toString();
        m_sink->abort();
        m_reply.reset();
        emit failed("");
        return;
    } else if (m_state == State::AbortedByUser) {
        qCDebug(DownloadLogC) << getUid().toString() << "Download aborted in previous step:" << m_url.toString();
        m_sink->abort();
        m_reply.reset();
        emit aborted();
        return;
    }

    // make sure we got all the remaining data, if any
    auto data = m_reply->readAll();
    if (data.size()) {
        qCDebug(DownloadLogC) << getUid().toString() << "Writing extra" << data.size() << "bytes";
        m_state = m_sink->write(data);
    }

    // otherwise, finalize the whole graph
    m_state = m_sink->finalize(*m_reply.get());
    if (m_state != State::Succeeded) {
        qCDebug(DownloadLogC) << getUid().toString() << "Download failed to finalize:" << m_url.toString();
        m_sink->abort();
        m_reply.reset();
        emit failed("");
        return;
    }

    m_reply.reset();
    qCDebug(DownloadLogC) << getUid().toString() << "Download succeeded:" << m_url.toString();
    emit succeeded();
}

void Download::downloadReadyRead()
{
    if (m_state == State::Running) {
        auto data = m_reply->readAll();
        m_state = m_sink->write(data);
        if (m_state == State::Failed) {
            qCCritical(DownloadLogC) << getUid().toString() << "Failed to process response chunk";
        }
        // qDebug() << "Download" << m_url.toString() << "gained" << data.size() << "bytes";
    } else {
        qCCritical(DownloadLogC) << getUid().toString() << "Cannot write download data! illegal status " << m_status;
    }
}

}  // namespace Net

auto Net::Download::abort() -> bool
{
    if (m_reply) {
        m_reply->abort();
    } else {
        m_state = State::AbortedByUser;
    }
    return true;
}
