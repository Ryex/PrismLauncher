// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PolyMC - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
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

#include "Application.h"

// #define BREAK_INFINITE_LOOP
// #define BREAK_EXCEPTION
// #define BREAK_RETURN

#ifdef BREAK_INFINITE_LOOP
#include <thread>
#include <chrono>
#endif

#include <fstream>
#include <iostream>

#include <QQuickStyle>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0) && QT_VERSION < QT_VERSION_CHECK(6, 4, 0)
#include <QQuickWindow>
#include <QSGRendererInterface>
#endif

/** We need this here because the environment variable MUST be set before creating
 *  the Qt Application object for the QCC style to be applied correctly.
 */
void bootstrapThemeEnvironment()
{
    std::ifstream qml_theme_file(".qml_theme");
    std::string line;
    if (qml_theme_file.is_open()) {
        if (std::getline(qml_theme_file, line))
            qputenv("QT_QUICK_CONTROLS_CONF", line.data());

        qml_theme_file.close();
    } else {
        std::cout << "No QML theme file could be found!" << std::endl;
    }
}

int main(int argc, char *argv[])
{
#ifdef BREAK_INFINITE_LOOP
    while(true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
#endif
#ifdef BREAK_EXCEPTION
    throw 42;
#endif
#ifdef BREAK_RETURN
    return 42;
#endif

#if QT_VERSION <= QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    bootstrapThemeEnvironment();

    // Avoids using the ugly default styles :|
    if (!qEnvironmentVariableIsSet("QT_QUICK_CONTROLS_CONF"))
        QQuickStyle::setStyle("Fusion");

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0) && QT_VERSION < QT_VERSION_CHECK(6, 4, 0)
    // Avoids crash on Qt6 < 6.4
    QQuickWindow::setGraphicsApi(QSGRendererInterface::GraphicsApi::OpenGL);
#endif

    // initialize Qt
    Application app(argc, argv);

    switch (app.status())
    {
    case Application::StartingUp:
    case Application::Initialized:
    {
        Q_INIT_RESOURCE(multimc);
        Q_INIT_RESOURCE(backgrounds);
        Q_INIT_RESOURCE(documents);
        Q_INIT_RESOURCE(prismlauncher);

        Q_INIT_RESOURCE(pe_dark);
        Q_INIT_RESOURCE(pe_light);
        Q_INIT_RESOURCE(pe_blue);
        Q_INIT_RESOURCE(pe_colored);
        Q_INIT_RESOURCE(breeze_dark);
        Q_INIT_RESOURCE(breeze_light);
        Q_INIT_RESOURCE(OSX);
        Q_INIT_RESOURCE(iOS);
        Q_INIT_RESOURCE(flat);
        Q_INIT_RESOURCE(flat_white);

        Q_INIT_RESOURCE(QMLResources);

        return app.exec();
    }
    case Application::Failed:
        return 1;
    case Application::Succeeded:
        return 0;
    default:
        return -1;
    }
}
