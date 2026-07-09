#pragma once
#include <QString>
#include <QStringList>
#include <QList>
#include <map>
#include <QSettings>
#include <QByteArray>

struct TableRowData {
    QString key;
    QString value;
    QString comment;
};

// Структура для возврата результата проверки (сама структура остаётся тут,
// это просто общий тип данных, а не логика валидации)
struct ValidationResult {
    bool isValid = true;
    QString errorMessage;
    enum class ErrorSource { None, Key };
    ErrorSource errorSource = ErrorSource::None;
};

// Интерфейс для работы с данными.
// Больше никакой валидации — только хранение и доступ к данным
class IGlobalVariant
{
public:
    virtual ~IGlobalVariant() = default;

    virtual void setValue(const QString& key, const QString& value, const QString& comment = QString()) = 0;
    virtual QString value(const QString& key, const QString& defaultValue = QString()) const = 0;

    virtual void setComment(const QString& key, const QString& comment) = 0;
    virtual QString comment(const QString& key) const = 0;

    virtual QList<TableRowData> getValues() const = 0;
    virtual void setValues(const QList<TableRowData>& data) = 0;
};

// Реализация интерфейса — без изменений
class GlobalVariant : public IGlobalVariant
{
public:
    GlobalVariant();

    void setValue(const QString& key, const QString& value, const QString& comment = QString()) override;
    QString value(const QString& key, const QString& defaultValue = QString()) const override;

    void setComment(const QString& key, const QString& comment) override;
    QString comment(const QString& key) const override;

    QList<TableRowData> getValues() const override;
    void setValues(const QList<TableRowData>& data) override;

    static QString getValue(const QString& key, const QString& defaultValue = QString());
    static QStringList getKeys();
private:
    void saveToSettings() const;
    void loadFromSettings();

    std::map<QString, TableRowData> m_data;
};