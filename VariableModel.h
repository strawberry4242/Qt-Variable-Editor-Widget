#pragma once
#include <QAbstractTableModel>
#include <QStyledItemDelegate>
#include <QUndoCommand>
#include <QList>
#include <QColor>
#include "GlobalVariant.h"

class QUndoStack;


// МОДЕЛЬ ТАБЛИЦЫ наследуется от QAbstractTableModel
// Все изменения синхронизируются с IGlobalVariant.

class VariableTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Columns { ColKey = 0, ColValue, ColComment, ColCount };
    explicit VariableTableModel(IGlobalVariant* manager, QObject* parent = nullptr);
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    // Undo-стек для правки прямо в ячейках таблицы 
    void setUndoStack(QUndoStack* stack) { m_undoStack = stack; }
    // Валидация здесь уже не выполняется  она должна быть пройдена до вызова (делегатом)
    void commitCell(int row, int col, const QString& text);
    void insertRow(int row, const TableRowData& data);
    void removeRow(int row);
    TableRowData rowAt(int row) const;
    void reload(); // перечитать данные из менеджера
private:
    void renameKeyInManager(const QString& oldKey, const QString& newKey);
    IGlobalVariant* m_manager;
    QList<TableRowData> m_rows;  // кэш строк для быстрого доступа по индексу
    QUndoStack* m_undoStack = nullptr;
};


// ДЕЛЕГАТ  подсветка поиска + валидация
// значения перед тем, как оно попадёт в модель 
class ValidatingDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit ValidatingDelegate(IGlobalVariant* manager, QObject* parent = nullptr);
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    bool eventFilter(QObject* editor, QEvent* event) override;
    void setSearchText(const QString& text) { m_searchText = text; }
    QString searchText() const { return m_searchText; }
    static ValidationResult validateKeyNotEmpty(const QString& key);
    static ValidationResult validateKeyFormat(const QString& key);
    static bool keyExists(IGlobalVariant* manager, const QString& key, const QString& oldKey = QString());
private:
    IGlobalVariant* m_manager;
    QString m_searchText;
};


// UNDO-КОМАНДЫ

class EditCellCommand : public QUndoCommand {
public:
    EditCellCommand(VariableTableModel* model, int row, int col,
        const QString& oldText, const QString& newText, QUndoCommand* parent = nullptr);
    void redo() override;
    void undo() override;
private:
    VariableTableModel* m_model;
    int m_row, m_col;
    QString m_oldText, m_newText;
};

class AddRowCommand : public QUndoCommand {
public:
    AddRowCommand(VariableTableModel* model, int row, const TableRowData& data, QUndoCommand* parent = nullptr);
    void redo() override;
    void undo() override;
private:
    VariableTableModel* m_model;
    int m_row;
    TableRowData m_data;
};

class DeleteRowCommand : public QUndoCommand {
public:
    DeleteRowCommand(VariableTableModel* model, int row, QUndoCommand* parent = nullptr);
    void redo() override;
    void undo() override;
private:
    VariableTableModel* m_model;
    int m_row;
    TableRowData m_data;
};