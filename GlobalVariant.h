#pragma once
#include <QString>
#include <QStringList>
#include <QList>
#include <QRegularExpression>
#include <map>
#pragma execution_character_set("utf-8")

struct TableRowData {
    QString key;
    QString value;
    QString comment;
};

// Структура для возврата результата проверки
struct ValidationResult {
    bool isValid = true;
    QString errorMessage;
    enum class ErrorSource { None, Key };  
    ErrorSource errorSource = ErrorSource::None;
};

// Интерфейс для работы с данными
class IGlobalVariant
{
public:
    virtual ~IGlobalVariant() = default;

    virtual void setValue(const QString& key, const QString& value, const QString& comment = QString()) = 0;
    virtual QString value(const QString& key) const = 0;

    // Работа с комментарием отдельно от значения 
    virtual void setComment(const QString& key, const QString& comment) = 0;
    virtual QString comment(const QString& key) const = 0;

    virtual QList<TableRowData> getValues() const = 0;
    virtual void setValues(const QList<TableRowData>& data) = 0;

    // Дефолтная реализация проверки ключа 
    virtual ValidationResult validateKey(const QString& key, int index = -1) {
        static const QRegularExpression keyRegex("^[a-zA-Z0-9_]+$");
        ValidationResult result;

        if (key.isEmpty()) {
            result.isValid = false;
            result.errorMessage = "Название (Ключ) не может быть пустым!";
            result.errorSource = ValidationResult::ErrorSource::Key;
            return result;
        }
        if (!keyRegex.match(key).hasMatch()) {
            result.isValid = false;
            result.errorMessage = "Ошибка в Ключе! Допустимы только латинские буквы, цифры и знак подчеркивания '_'.";
            result.errorSource = ValidationResult::ErrorSource::Key;
            return result;
        }

        const QList<TableRowData> currentValues = getValues();
        for (int i = 0; i < currentValues.size(); ++i) {
            if (i == index) continue;
            if (currentValues[i].key == key) {
                result.isValid = false;
                result.errorMessage = "Переменная с именем '" + key + "' уже существует!";
                result.errorSource = ValidationResult::ErrorSource::Key;
                return result;
            }
        }
        return result;
    }
};

// Реализация интерфейса
class GlobalVariant : public IGlobalVariant
{
public:
    GlobalVariant();

    void setValue(const QString& key, const QString& value, const QString& comment = QString()) override;
    QString value(const QString& key) const override;

    void setComment(const QString& key, const QString& comment) override;
    QString comment(const QString& key) const override;

    QList<TableRowData> getValues() const override;
    void setValues(const QList<TableRowData>& data) override;

    static QString getValue(const QString& key, const QString& defaultValue = QString()); // значение по ключу
    static QStringList getKeys(); // список доступных ключей
private:
    void saveToSettings() const;
    void loadFromSettings();

    // Хранилище переменных: ключ -> данные строки (п.10 вместо QList)
    std::map<QString, TableRowData> m_data;
};