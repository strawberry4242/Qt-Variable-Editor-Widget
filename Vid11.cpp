#include "Vid11.h"
#include "GlobalVariant.h"
#include "VariableModel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTableView>
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>
#include <QUndoStack>
#include <QShortcut>
#include <QMessageBox>
#include <QPixmap>
#include <QPainter>
#include <QPen>
#include <QIcon>
#include <QStyle>
#include <QDialog>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QTimer>
#include <QItemSelectionModel>
#include <QDebug>


Vid11::Vid11(QWidget* parent)
    : QWidget(parent)
{
    // Виджет создаёт и хранит GlobalVariant, дальше передаёт указатель
    // на него в модель, делегат и диалог добавления новой строки 
    m_variantManager = new GlobalVariant();

    m_undoStack = new QUndoStack(this);
    m_undoStack->setUndoLimit(50);

    m_model = new VariableTableModel(m_variantManager, this);
    m_model->setUndoStack(m_undoStack);

    // Модель оборачиваем в прокси на ней  фильтрация и сортировка 
    m_proxyModel = new VariableFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_model);

    m_delegate = new ValidatingDelegate(m_variantManager, this);

    setupUI();

    connect(m_undoStack, &QUndoStack::canUndoChanged, Buttonundo, &QPushButton::setEnabled);
    connect(m_undoStack, &QUndoStack::canRedoChanged, Buttonredo, &QPushButton::setEnabled);
    Buttonundo->setEnabled(false);
    Buttonredo->setEnabled(false);

    // Горячие клавиши
    QShortcut* deleteKey = new QShortcut(QKeySequence(Qt::Key_Delete), this);
    connect(deleteKey, &QShortcut::activated, this, &Vid11::DeleteClicked);

    QShortcut* addKey = new QShortcut(QKeySequence("Shift+R"), this);
    connect(addKey, &QShortcut::activated, this, &Vid11::AddClicked);

    QShortcut* undoKey = new QShortcut(QKeySequence("Ctrl+Z"), this);
    connect(undoKey, &QShortcut::activated, this, &Vid11::UndoClicked);
    QShortcut* redoKey = new QShortcut(QKeySequence("Ctrl+Shift+Z"), this);
    connect(redoKey, &QShortcut::activated, this, &Vid11::RedoClicked);

    // Автоматически выделяем и прокручиваем к строке, которую добавили или отредактировали 
    connect(m_model, &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex&, int first, int) {
        QTimer::singleShot(0, this, [this, first]() { highlightSourceRow(first); });
        });
    connect(m_model, &QAbstractItemModel::dataChanged, this,
        [this](const QModelIndex& topLeft, const QModelIndex&, const QVector<int>&) {
            const int row = topLeft.row();
            QTimer::singleShot(0, this, [this, row]() { highlightSourceRow(row); });
        });
}

Vid11::~Vid11() {
    delete m_variantManager;
}

void Vid11::setupUI() {
    this->setWindowTitle("Редактор внутренних переменных");
    this->resize(750, 500);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(9, 9, 9, 9);
    mainLayout->setSpacing(6);

    // Блок поиска
    QHBoxLayout* searchLayout = new QHBoxLayout();
    QLabel* searchLabel = new QLabel("Поиск:", this);
    lineEdit_search = new QLineEdit(this);
    lineEdit_search->setPlaceholderText("Введите текст для поиска...");
    searchLayout->addWidget(searchLabel);
    searchLayout->addWidget(lineEdit_search);
    mainLayout->addLayout(searchLayout);

    // Блок кнопок
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(6);

    Button_add = new QPushButton("Добавить строку", this);
    Button_delete = new QPushButton("Удалить строку", this);
    Buttonundo = new QPushButton("Отменить", this);
    Buttonredo = new QPushButton("Повторить", this);

    QList<QPushButton*> allButtons = { Button_add, Button_delete, Buttonundo, Buttonredo };
    for (QPushButton* btn : allButtons) {
        btn->setCursor(Qt::PointingHandCursor);
    }

    // Иконка плюса
    QPixmap plusPixmap(16, 16);
    plusPixmap.fill(Qt::transparent);
    QPainter painter(&plusPixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(0, 150, 0), 2.0));
    painter.drawLine(8, 2, 8, 14);
    painter.drawLine(2, 8, 14, 8);
    painter.end();
    Button_add->setIcon(QIcon(plusPixmap));

    QSize iconSize(16, 16);
    Button_add->setIconSize(iconSize);
    Buttonundo->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
    Buttonundo->setIconSize(iconSize);
    Buttonredo->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
    Buttonredo->setIconSize(iconSize);
    Button_delete->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));
    Button_delete->setIconSize(iconSize);

    buttonLayout->addWidget(Button_add);
    buttonLayout->addStretch();
    buttonLayout->addWidget(Button_delete);
    buttonLayout->addWidget(Buttonundo);
    buttonLayout->addWidget(Buttonredo);
    mainLayout->addLayout(buttonLayout);

    // Таблица теперь QTableView + модель 
    tableView = new QTableView(this);
    tableView->setModel(m_proxyModel);
    tableView->setItemDelegate(m_delegate);
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tableView->setSortingEnabled(true); // сортировка через прокси-модель
    m_proxyModel->sort(VariableTableModel::ColKey, Qt::AscendingOrder); // сортировка по ключу сразу, чтобы новые строки автоматически вставали на место
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    mainLayout->addWidget(tableView);

    connect(Button_add, &QPushButton::clicked, this, &Vid11::AddClicked);
    connect(Button_delete, &QPushButton::clicked, this, &Vid11::DeleteClicked);
    connect(Buttonundo, &QPushButton::clicked, this, &Vid11::UndoClicked);
    connect(Buttonredo, &QPushButton::clicked, this, &Vid11::RedoClicked);
    connect(lineEdit_search, &QLineEdit::textChanged, this, &Vid11::OnSearchChanged);
}

// Находит строку по индексу исхоной модели в уже отсортированной/отфильтрованной
// прокси-модели, выделяет её и прокручивает к ней таблицу
void Vid11::highlightSourceRow(int sourceRow) {
    if (sourceRow < 0 || sourceRow >= m_model->rowCount()) return;

    QModelIndex sourceIndex = m_model->index(sourceRow, VariableTableModel::ColKey);
    QModelIndex proxyIndex = m_proxyModel->mapFromSource(sourceIndex);

    // Строка могла быть скрыта активным фильтром поиска  тогда просто ничего не делаем
    if (!proxyIndex.isValid()) return;

    tableView->setCurrentIndex(proxyIndex);
    tableView->selectionModel()->select(proxyIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    tableView->scrollTo(proxyIndex, QAbstractItemView::PositionAtCenter);
}
// При нажатии 'Добавить'
void Vid11::AddClicked() {
    QDialog dialog(this);
    dialog.setWindowTitle("Новая переменная");
    dialog.setMinimumWidth(400);

    QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setContentsMargins(11, 11, 11, 11);
    mainLayout->setSpacing(8);

    QFormLayout* formLayout = new QFormLayout();
    formLayout->setLabelAlignment(Qt::AlignLeft);
    formLayout->setSpacing(8);

    QLineEdit* keyEdit = new QLineEdit(&dialog);
    keyEdit->setPlaceholderText("Например: timeout_10");
    formLayout->addRow("Название (Ключ):", keyEdit);

    QLineEdit* valueEdit = new QLineEdit(&dialog);
    valueEdit->setPlaceholderText("Необязательно");
    formLayout->addRow("Значение:", valueEdit);

    // Возможность ввести комментарий (п.8)
    QLineEdit* commentEdit = new QLineEdit(&dialog);
    commentEdit->setPlaceholderText("Необязательно");
    formLayout->addRow("Комментарий:", commentEdit);

    mainLayout->addLayout(formLayout);

    QLabel* errorLabel = new QLabel(&dialog);
    errorLabel->setStyleSheet("color: red; font-weight: bold;");
    errorLabel->setWordWrap(true);
    mainLayout->addWidget(errorLabel);
    mainLayout->addStretch(1);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    mainLayout->addWidget(buttonBox);
    QPushButton* okButton = buttonBox->button(QDialogButtonBox::Ok);

    // Валидация в реальном времени и блокировка OK, пока есть ошибка (п.9)
    auto validateLive = [=]() {
        ValidationResult keyResult = m_variantManager->validateKey(keyEdit->text().trimmed());
        keyEdit->setStyleSheet(keyResult.isValid ? "" : "border: 1px solid red; background-color: #FFF0F0;");
        if (!keyResult.isValid) {
            errorLabel->setText(keyResult.errorMessage);
        }
        else {
            errorLabel->clear();
        }

        okButton->setEnabled(keyResult.isValid);
        };

    connect(keyEdit, &QLineEdit::textChanged, validateLive);
    connect(valueEdit, &QLineEdit::textChanged, validateLive);

    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    validateLive(); // начальное состояние - поля пустые, OK выключен

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    TableRowData newData;
    newData.key = keyEdit->text().trimmed();
    newData.value = valueEdit->text().trimmed();
    newData.comment = commentEdit->text().trimmed();

    int targetRow = m_model->rowCount();
    m_undoStack->push(new AddRowCommand(m_model, targetRow, newData));
}

// При нажатии 'Удалить'
void Vid11::DeleteClicked() {
    QModelIndex proxyIndex = tableView->currentIndex();
    if (!proxyIndex.isValid()) return;

    // Индекс из view идёт через прокси  переводим в индекс исходной модели
    QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);
    m_undoStack->push(new DeleteRowCommand(m_model, sourceIndex.row()));
}

void Vid11::OnSearchChanged(const QString& text) {
    m_delegate->searchText = text;
    // Фильтрация делается прокси-моделью через собственное поле поиска 
    m_proxyModel->setSearchText(text);
    tableView->viewport()->update();
}

void Vid11::UndoClicked() {
    m_undoStack->undo();
}

void Vid11::RedoClicked() {
    m_undoStack->redo();
}