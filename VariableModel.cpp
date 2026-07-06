#include "VariableModel.h"
#include <QUndoStack>
#include <QLineEdit>
#include <QMessageBox>
#include <QTextDocument>
#include <QPainter>
#include <QToolTip>
#include <QStyle>

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

//  VariableFilterProxyModel

VariableFilterProxyModel::VariableFilterProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
}

void VariableFilterProxyModel::setSearchText(const QString& text) {
    if (m_searchText == text) return;
    m_searchText = text;
    invalidateFilter(); // заставляет прокси заново прогнать filterAcceptsRow по всем строкам
}

bool VariableFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const {
    if (m_searchText.isEmpty()) return true;

    QAbstractItemModel* src = sourceModel();
    for (int col = 0; col < src->columnCount(); ++col) {
        const QModelIndex idx = src->index(sourceRow, col, sourceParent);
        if (idx.data().toString().contains(m_searchText, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

//  ValidatingDelegate 

ValidatingDelegate::ValidatingDelegate(IGlobalVariant* manager, QObject* parent)
    : QStyledItemDelegate(parent), m_manager(manager)
{
}

QWidget* ValidatingDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    Q_UNUSED(option);
    auto* editor = new QLineEdit(parent);

    // Комментарий и значение не проверяем  живая подсветка нужна только ключу 
    if (index.column() == VariableTableModel::ColComment || index.column() == VariableTableModel::ColValue) {
        return editor;
    }

    const int col = index.column();
    const int row = index.row();

    // Валидация на лету, пока пользователь печатает подсветка красным
    // и всплывающая подсказка с текстом ошибки
    connect(editor, &QLineEdit::textChanged, editor, [this, editor, col, row](const QString& text) {
        ValidationResult result;
        const QString trimmed = text.trimmed();
        if (col == VariableTableModel::ColKey) {
            result = m_manager->validateKey(trimmed, row);
        }

        if (!result.isValid) {
            editor->setStyleSheet("border: 2px solid red; background-color: #FFF0F0;");
            QToolTip::showText(editor->mapToGlobal(QPoint(0, editor->height())), result.errorMessage, editor);
        }
        else {
            editor->setStyleSheet("");
            QToolTip::hideText();
        }
        });

    return editor;
}

//  проверяется валидность значения до того, как оно
// попадёт в модель. Если проверка не пройдена  в модель ничего не пишем.
void ValidatingDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const {
    auto* lineEdit = qobject_cast<QLineEdit*>(editor);
    if (!lineEdit) {
        QStyledItemDelegate::setModelData(editor, model, index);
        return;
    }

    const QString text = lineEdit->text().trimmed();
    const int col = index.column();

    // Комментарий и значение валидировать не нужно
    if (col == VariableTableModel::ColComment) {
        model->setData(index, text, Qt::EditRole);
        return;
    }

    if (col == VariableTableModel::ColValue) {
        model->setData(index, text, Qt::EditRole);
        return;
    }

    ValidationResult result;
    if (col == VariableTableModel::ColKey) {
        result = m_manager->validateKey(text, index.row());
    }

    if (!result.isValid) {
        // Ошибка уже показана пользователю живой подсветкой в createEditor 
        // здесь не коммитим невалидное значение, ячейка останется прежней
        QToolTip::showText(editor->mapToGlobal(QPoint(0, editor->height())), result.errorMessage, editor);
        return;
    }

    model->setData(index, text, Qt::EditRole);
}

void ValidatingDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    const QString text = index.data().toString();
    if (!searchText.isEmpty() && text.contains(searchText, Qt::CaseInsensitive)) {
        QTextDocument doc;
        QString highlighted = text;
        highlighted.replace(searchText, "<span style='background-color:yellow; color:black;'>" + searchText + "</span>", Qt::CaseInsensitive);
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