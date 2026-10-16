#include "CopyCatalog.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include <stdexcept>

static void initializeCopyResource()
{
    static const bool initialized = [] { Q_INIT_RESOURCE(copy); return true; }();
    Q_UNUSED(initialized);
}

CopyCatalog::CopyCatalog()
{
    initializeCopyResource();
    QFile file(":/daemon/copy/ko.json");
    if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Missing bundled dialogue catalog");
    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) throw std::runtime_error("Invalid bundled dialogue catalog");
    const auto object = document.object();
    for (auto it = object.begin(); it != object.end(); ++it) {
        if (!it.value().isArray() || it.value().toArray().isEmpty())
            throw std::runtime_error("Invalid bundled dialogue pool");
        QStringList pool;
        for (const auto& value : it.value().toArray()) {
            if (!value.isString() || value.toString().trimmed().isEmpty())
                throw std::runtime_error("Invalid bundled dialogue line");
            pool.push_back(value.toString());
        }
        m_pools.insert(it.key(), pool);
    }
}

QString CopyCatalog::pick(const QString& key, const std::function<int(int)>& choose)
{
    auto candidates = m_pools.value(key);
    if (candidates.isEmpty()) return {};
    if (candidates.size() > 1) candidates.removeAll(m_last.value(key));
    const int count = static_cast<int>(candidates.size());
    const int index = choose ? std::clamp(choose(count), 0, count - 1) : 0;
    const auto line = candidates[index];
    m_last[key] = line;
    return line;
}
