#include "VariableModel.h"
#include <QUndoStack>
#include <QLineEdit>
#include <QMessageBox>
#include <QTextDocument>
#include <QPainter>
#include <QToolTip>
#include <QStyle>
#include <QKeyEvent>

// VariableTableModel 

VariableTableModel::VariableTableModel(IGlobalVariant* manager, QObject* parent)
    : QAbstractTableModel(parent), m_manager(manager)
{
    reload();
}

void VariableTableModel::reload() {
    beginResetModel();
    m_rows = m_manager->getValues();
    endResetModel();
}

int VariableTableModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_rows.size();
}

int VariableTableModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : ColCount;
}

QVariant VariableTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_rows.size()) return QVariant();
    if (role != Qt::DisplayRole && role != Qt::EditRole) return QVariant();

    const TableRowData& row = m_rows.at(index.row());
    switch (index.column()) {
    case ColKey: return row.key;
    case ColValue: return row.value;
    case ColComment: return row.comment;
    default: return QVariant();
    }
}

bool VariableTableModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid() || role != Qt::EditRole || index.row() >= m_rows.size()) {
        return false;
    }

    const QString newText = value.toString();
    QString oldText;
    switch (index.column()) {
    case ColKey: oldText = m_rows[index.row()].key; break;
    case ColValue: oldText = m_rows[index.row()].value; break;
    case ColComment: oldText = m_rows[index.row()].comment; break;
    default: return false;
    }
    if (oldText == newText) return false;

    // Любая правка ячейки идёт через undo-команду  тогда Ctrl+Z работает
    // и для редактирования прямо в таблице, а не только для кнопок
    if (m_undoStack) {
        m_undoStack->push(new EditCellCommand(this, index.row(), index.column(), oldText, newText));
    }
    else {
        commitCell(index.row(), index.column(), newText);
    }
    return true;
}

QVariant VariableTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) return QVariant();
    switch (section) {
    case ColKey:     return QStringLiteral("Ключ");
    case ColValue:   return QStringLiteral("Значение");
    case ColComment: return QStringLiteral("Комментарий");
    default:         return QVariant();
    }
}

Qt::ItemFlags VariableTableModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

void VariableTableModel::commitCell(int row, int col, const QString& text) {
    if (row < 0 || row >= m_rows.size()) return;
    const QString currentKey = m_rows[row].key;

    switch (col) {
    case ColKey:
        renameKeyInManager(currentKey, text);
        m_rows[row].key = text;
        break;
    case ColValue:
        m_manager->setValue(currentKey, text, m_rows[row].comment);
        m_rows[row].value = text;
        break;
    case ColComment:
        m_manager->setComment(currentKey, text);
        m_rows[row].comment = text;
        break;
    default:
        return;
    }
    emit dataChanged(index(row, col), index(row, col));
}

void VariableTableModel::renameKeyInManager(const QString& oldKey, const QString& newKey) {
    QList<TableRowData> all = m_manager->getValues();
    for (auto& row : all) {
        if (row.key == oldKey) {
            row.key = newKey;
            break;
        }
    }
    m_manager->setValues(all);
}

void VariableTableModel::insertRow(int row, const TableRowData& data) {
    row = qBound(0, row, m_rows.size());
    beginInsertRows(QModelIndex(), row, row);
    m_rows.insert(row, data);
    endInsertRows();

    QList<TableRowData> all = m_manager->getValues();
    all.insert(qBound(0, row, all.size()), data);
    m_manager->setValues(all);
}

void VariableTableModel::removeRow(int row) {
    if (row < 0 || row >= m_rows.size()) return;

    const QString key = m_rows[row].key;
    beginRemoveRows(QModelIndex(), row, row);
    m_rows.removeAt(row);
    endRemoveRows();

    QList<TableRowData> all = m_manager->getValues();
    for (int i = 0; i < all.size(); ++i) {
        if (all[i].key == key) {
            all.removeAt(i);
            break;
        }
    }
    m_manager->setValues(all);
}

TableRowData VariableTableModel::rowAt(int row) const {
    return (row >= 0 && row < m_rows.size()) ? m_rows.at(row) : TableRowData();
}

//  ValidatingDelegate 
ValidatingDelegate::ValidatingDelegate(IGlobalVariant* manager, QObject* parent)
    : QStyledItemDelegate(parent), m_manager(manager)
{
}

ValidationResult ValidatingDelegate::validateKeyNotEmpty(const QString& key) {
    ValidationResult result;
    if (key.isEmpty()) {
        result.isValid = false;
        result.errorMessage = "Название (Ключ) не может быть пустым!";
        result.errorSource = ValidationResult::ErrorSource::Key;
    }
    return result;
}

ValidationResult ValidatingDelegate::validateKeyFormat(const QString& key) {
    static const QRegularExpression keyRegex("^[a-zA-Z0-9_]+$");
    ValidationResult result;
    if (!keyRegex.match(key).hasMatch()) {
        result.isValid = false;
        result.errorMessage = "Ошибка в Ключе! Допустимы только латинские буквы, цифры и знак подчеркивания '_'.";
        result.errorSource = ValidationResult::ErrorSource::Key;
    }
    return result;
}

bool ValidatingDelegate::keyExists(IGlobalVariant* manager, const QString& key, const QString& oldKey) {
    if (key == oldKey) return false; // ключ не поменялся — сам с собой не дубликат
    const QList<TableRowData> values = manager->getValues();
    for (const auto& row : values) {
        if (row.key == key) return true;
    }
    return false;
}

class MyLineEdit : public QLineEdit
{
public:
    explicit MyLineEdit(IGlobalVariant* manager, QWidget* parent)
        : QLineEdit(parent), m_manager(manager)
    {
        connect(this, &QLineEdit::textChanged, this, [this](const QString& text) {
            const QString trimmed = text.trimmed();

            ValidationResult result = ValidatingDelegate::validateKeyNotEmpty(trimmed);
            if (result.isValid) {
                result = ValidatingDelegate::validasteKeyFormat(trimmed);
            }

            if (!result.isValid) {
                setStyleSheet("border: 2px solid red; background-color: #FFF0F0;");
                QToolTip::showText(mapToGlobal(QPoint(0, height())), result.errorMessage, this);
            }
            else {
                setStyleSheet("");
                QToolTip::hideText();
            }
            });
    }

    void setDefaultValue(const QString& text)
    {
        setText(text);
        m_defaultValue = text;
    }

    QString defaultValue() const { return m_defaultValue; }

    bool isDefaultValue() const
    {
        return text().trimmed() == m_defaultValue;
    }

private:
    IGlobalVariant* m_manager;
    QString m_defaultValue;
};

QWidget* ValidatingDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    Q_UNUSED(option);
    if (index.column() != VariableTableModel::ColKey)
        return QStyledItemDelegate::createEditor(parent, option, index);

    return new MyLineEdit(m_manager, parent);
}

void ValidatingDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    if (index.column() != VariableTableModel::ColKey) {
        QStyledItemDelegate::setEditorData(editor, index);
        return;
    }

    auto* lineEdit = dynamic_cast<MyLineEdit*>(editor);
    if (!lineEdit) {
        QStyledItemDelegate::setEditorData(editor, index);
        return;
    }

    lineEdit->setDefaultValue(index.data().toString());
}

bool ValidatingDelegate::eventFilter(QObject* editor, QEvent* event) {
    auto* lineEdit = dynamic_cast<MyLineEdit*>(editor);
    if (!lineEdit)
        return QStyledItemDelegate::eventFilter(editor, event);

    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            const QString text = lineEdit->text().trimmed();

            ValidationResult result = validateKeyNotEmpty(text);
            if (result.isValid) result = validateKeyFormat(text);

            if (!result.isValid) {
                QToolTip::showText(lineEdit->mapToGlobal(QPoint(0, lineEdit->height())),
                    result.errorMessage, lineEdit);
                return true;
            }

            if (keyExists(m_manager, text, lineEdit->defaultValue())) {
                QToolTip::showText(lineEdit->mapToGlobal(QPoint(0, lineEdit->height())),
                    "Переменная с именем '" + text + "' уже существует!", lineEdit);
                return true;
            }
        }
    }
    else if (event->type() == QEvent::FocusOut) {
        const QString text = lineEdit->text().trimmed();
        const bool notEmptyOk = validateKeyNotEmpty(text).isValid;
        const bool formatOk = notEmptyOk && validateKeyFormat(text).isValid;
        const bool duplicate = keyExists(m_manager, text, lineEdit->defaultValue());

        if (!formatOk || duplicate) {
            lineEdit->setText(lineEdit->defaultValue());
            lineEdit->setStyleSheet("");
            QToolTip::hideText();
        }
    }
    return QStyledItemDelegate::eventFilter(editor, event);
}

void ValidatingDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const {
    auto* lineEdit = qobject_cast<QLineEdit*>(editor);
    if (!lineEdit) {
        QStyledItemDelegate::setModelData(editor, model, index);
        return;
    }
    const QString text = lineEdit->text().trimmed();
    model->setData(index, text, Qt::EditRole);
}

void ValidatingDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    const QString text = index.data().toString();
    if (!m_searchText.isEmpty() && text.contains(m_searchText, Qt::CaseInsensitive)) {
        QTextDocument doc;
        QString highlighted = text;
        QColor searchHighlightColor = QColor(Qt::yellow);
        QColor searchHighlightTextColor = QColor(Qt::black);
        const QString spanStyle = QString("background-color:%1; color:%2;").arg(searchHighlightColor.name(), searchHighlightTextColor.name());
        highlighted.replace(m_searchText, "<span style='" + spanStyle + "'>" + m_searchText + "</span>", Qt::CaseInsensitive);
        doc.setHtml(highlighted);

        painter->save();
        painter->translate(option.rect.topLeft());
        doc.drawContents(painter);
        painter->restore();
    }
    else {
        QStyledItemDelegate::paint(painter, option, index);
    }
}

// Undo-команды

EditCellCommand::EditCellCommand(VariableTableModel* model, int row, int col,
    const QString& oldText, const QString& newText, QUndoCommand* parent)
    : QUndoCommand(parent), m_model(model), m_row(row), m_col(col), m_oldText(oldText), m_newText(newText)
{
    setText(QString("Изменение ячейки (%1, %2)").arg(row).arg(col));
}
void EditCellCommand::redo() { m_model->commitCell(m_row, m_col, m_newText); }
void EditCellCommand::undo() { m_model->commitCell(m_row, m_col, m_oldText); }

AddRowCommand::AddRowCommand(VariableTableModel* model, int row, const TableRowData& data, QUndoCommand* parent)
    : QUndoCommand(parent), m_model(model), m_row(row), m_data(data)
{
    setText("Добавление строки");
}
void AddRowCommand::redo() { m_model->insertRow(m_row, m_data); }
void AddRowCommand::undo() { m_model->removeRow(m_row); }

DeleteRowCommand::DeleteRowCommand(VariableTableModel* model, int row, QUndoCommand* parent)
    : QUndoCommand(parent), m_model(model), m_row(row)
{
    setText("Удаление строки");
    m_data = model->rowAt(row);
}
void DeleteRowCommand::redo() { m_model->removeRow(m_row); }
void DeleteRowCommand::undo() { m_model->insertRow(m_row, m_data); }