#pragma once
#include <QWidget>

// в хедере не видно, какие классы используются внутри реализации 
class QLineEdit;
class QPushButton;
class QTableView;
class QUndoStack;
class IGlobalVariant;
class VariableTableModel;
class ValidatingDelegate;

class Vid11 : public QWidget
{
    Q_OBJECT
public:
    explicit Vid11(QWidget* parent = nullptr);
    ~Vid11();

private slots:
    void AddClicked();
    void DeleteClicked();
    void OnSearchChanged(const QString& text);
    void UndoClicked();
    void RedoClicked();

private:
    void setupUI();

    QTableView* tableView;
    QLineEdit* lineEdit_search;
    QPushButton* Button_add;
    QPushButton* Button_delete;
    QPushButton* Buttonundo;
    QPushButton* Buttonredo;

    QUndoStack* m_undoStack;
    IGlobalVariant* m_variantManager;
    VariableTableModel* m_model;
    class QSortFilterProxyModel* m_proxyModel;
    ValidatingDelegate* m_delegate;
protected:
    // для перехвата закрытия окна:
    void closeEvent(QCloseEvent* event) override;
};