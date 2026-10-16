#pragma once
#include <QMap>
#include <QStringList>
#include <functional>

class CopyCatalog {
public:
    CopyCatalog();
    QString pick(const QString& key, const std::function<int(int)>& choose);
private:
    QMap<QString, QStringList> m_pools;
    QMap<QString, QString> m_last;
};
