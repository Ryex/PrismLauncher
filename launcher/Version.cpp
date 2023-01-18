#include "Version.h"

#include <QUrl>
#include <QRegularExpression>
#include <QRegularExpressionMatch>

Version::Version(const QString &str) : m_string(str)
{
    parse();
}

bool Version::operator<(const Version &other) const
{
    const int size = qMax(m_sections.size(), other.m_sections.size());
    for (int i = 0; i < size; ++i)
    {
        const Section sec1 = (i >= m_sections.size()) ? Section("0") : m_sections.at(i);
        const Section sec2 =
            (i >= other.m_sections.size()) ? Section("0") : other.m_sections.at(i);
        if (sec1 != sec2)
        {
            return sec1 < sec2;
        }
    }

    return false;
}
bool Version::operator<=(const Version &other) const
{
    return *this < other || *this == other;
}
bool Version::operator>(const Version &other) const
{
    const int size = qMax(m_sections.size(), other.m_sections.size());
    for (int i = 0; i < size; ++i)
    {
        const Section sec1 = (i >= m_sections.size()) ? Section("0") : m_sections.at(i);
        const Section sec2 =
            (i >= other.m_sections.size()) ? Section("0") : other.m_sections.at(i);
        if (sec1 != sec2)
        {
            return sec1 > sec2;
        }
    }

    return false;
}
bool Version::operator>=(const Version &other) const
{
    return *this > other || *this == other;
}
bool Version::operator==(const Version &other) const
{
    const int size = qMax(m_sections.size(), other.m_sections.size());
    for (int i = 0; i < size; ++i)
    {
        const Section sec1 = (i >= m_sections.size()) ? Section("0") : m_sections.at(i);
        const Section sec2 =
            (i >= other.m_sections.size()) ? Section("0") : other.m_sections.at(i);
        if (sec1 != sec2)
        {
            return false;
        }
    }

    return true;
}
bool Version::operator!=(const Version &other) const
{
    return !operator==(other);
}

void Version::parse()
{
    m_sections.clear();
    QString currentSection;

    auto classChange = [](QChar lastChar, QChar currentChar) {
        return !lastChar.isNull() && ((!lastChar.isDigit() && currentChar.isDigit()) || (lastChar.isDigit() && !currentChar.isDigit()));
    };

    for (int i = 0; i < m_string.size(); ++i) {
        const auto& current_char = m_string.at(i);
        if ((i > 0 && classChange(m_string.at(i - 1), current_char)) || current_char == '.' || current_char == '-' || current_char == '+') {
            if (!currentSection.isEmpty()) {
                m_sections.append(Section(currentSection));
            }
            currentSection = "";
        }
        currentSection += current_char;
    }
    if (!currentSection.isEmpty()) {
        m_sections.append(Section(currentSection));
    }
}


/// qDebug print support for the BlockedMod struct
QDebug operator<<(QDebug debug, const Version& v)
{
    QDebugStateSaver saver(debug);

    debug.nospace() << "Version{ string: " << v.toString() << ", sections: [ ";

    for (auto s : v.m_sections) {
        debug.nospace() << s.m_fullString << ", ";
    }
                    
    debug.nospace() << " ]" << " }";

    return debug;
}