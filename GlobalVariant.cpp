#include "GlobalVariant.h"
#include <QSettings>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
const QString SettingsKey = "variables_json";
const QString Prefix = "__VID_";
const QString Suffix = "__";

GlobalVariant::GlobalVariant() {
    loadFromSettings();
}

void GlobalVariant::setValue(const QString& key, const QString& value, const QString& comment) {
    auto it = m_data.find(key);
    if (it != m_data.end()) {
        it->second.value = value;
        if (!comment.isEmpty() || it->second.comment.isEmpty()) {
            it->second.comment = comment;
        }
    }
    else {
        TableRowData row;
        row.key = key;
        row.value = value;
        row.comment = comment;
        m_data.emplace(key, row);
    }
    saveToSettings();
}

QString GlobalVariant::value(const QString& key, const QString& defaultValue) const {
    auto it = m_data.find(key);
    return it != m_data.end() ? it->second.value : defaultValue;
}

void GlobalVariant::setComment(const QString& key, const QString& comment) {
    auto it = m_data.find(key);
    if (it != m_data.end()) {
        it->second.comment = comment;
        saveToSettings();
    }
}

QString GlobalVariant::comment(const QString& key) const {
    auto it = m_data.find(key);
    return it != m_data.end() ? it->second.comment : QString();
}

QList<TableRowData> GlobalVariant::getValues() const {
    QList<TableRowData> result;
    result.reserve(static_cast<int>(m_data.size()));
    for (const auto& pair : m_data) {
        result.append(pair.second);
    }
    return result;
}

void GlobalVariant::setValues(const QList<TableRowData>& data) {
    m_data.clear();
    for (const auto& row : data) {
        m_data.emplace(row.key, row);
    }
    saveToSettings();
}

// Храним весь набор переменных одной json-строкой в QSettings
void GlobalVariant::saveToSettings() const {
    QJsonArray arr;
    for (const auto& pair : m_data) {
        QJsonObject obj;
        // Обёртываем ключ при сохранении, не видно пользователю но видно в реестре
        QString wrappedKey = Prefix + pair.second.key + Suffix;
        obj["key"] = wrappedKey;
        obj["value"] = pair.second.value;
        obj["comment"] = pair.second.comment;
        arr.append(obj);
    }
    QJsonDocument doc(arr);

    QSettings settings; // Имя организации/приложения задаётся один раз в main.cpp
    settings.setValue(SettingsKey, QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}

void GlobalVariant::loadFromSettings() {
    m_data.clear();
    QSettings settings; // Имя организации/приложения задаётся один раз в main.cpp
    const QString raw = settings.value(SettingsKey).toString();
    if (raw.isEmpty()) return;

    QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8());
    if (!doc.isArray()) return;

    for (const QJsonValue& v : doc.array()) {
        const QJsonObject obj = v.toObject();
        TableRowData row;
        QString rawKey = obj.value("key").toString();

        // Убираем обёртки если присутствуют
        if (rawKey.startsWith(Prefix) && rawKey.endsWith(Suffix)) {
            rawKey = rawKey.mid(Prefix.length(), rawKey.length() - Prefix.length() - Suffix.length());
        }

        row.key = rawKey;
        row.value = obj.value("value").toString();
        row.comment = obj.value("comment").toString();
        if (!row.key.isEmpty()) {
            m_data.emplace(row.key, row);
        }
    }
}

QString GlobalVariant::getValue(const QString& key, const QString& defaultValue) {
    QSettings settings; // Имя организации/приложения задаётся один раз в main.cpp
    const QString raw = settings.value(SettingsKey).toString();
    if (raw.isEmpty()) return defaultValue;

    QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8());
    if (!doc.isArray()) return defaultValue;

    for (const QJsonValue& v : doc.array()) {
        const QJsonObject obj = v.toObject();
        QString rawKey = obj.value("key").toString();

        // Убираем обёртки для сравнения с искомым ключом
        if (rawKey.startsWith(Prefix) && rawKey.endsWith(Suffix)) {
            rawKey = rawKey.mid(Prefix.length(), rawKey.length() - Prefix.length() - Suffix.length());
        }

        if (rawKey == key) {
            return obj.value("value").toString();
        }
    }
    return defaultValue;
}

QStringList GlobalVariant::getKeys() {
    QStringList keys;
    QSettings settings; // Имя организации/приложения задаётся один раз в main.cpp
    const QString raw = settings.value(SettingsKey).toString();
    if (raw.isEmpty()) return keys;

    QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8());
    if (!doc.isArray()) return keys;

    for (const QJsonValue& v : doc.array()) {
        QString rawKey = v.toObject().value("key").toString();

        // Убираем обёртки для получения чистого ключа
        if (rawKey.startsWith(Prefix) && rawKey.endsWith(Suffix)) {
            rawKey = rawKey.mid(Prefix.length(), rawKey.length() - Prefix.length() - Suffix.length());
        }

        if (!rawKey.isEmpty()) keys.append(rawKey);
    }
    return keys;
}