#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QTableView>
#include <QDebug>

//#include <iostream>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    /*
     * connect buttons' singals
     */
    connect(ui->aboutAction, SIGNAL(triggered()), this, SLOT(about()));
    connect(ui->openAction, SIGNAL(triggered()), this, SLOT(about_to_open_file()));

    connect(ui->timer_runButton, SIGNAL(clicked()), this, SLOT(timer_run_restart()));
    connect(ui->timer_stopButton, SIGNAL(clicked()), this, SLOT(timer_run_stop()));

    connect(ui->step_runButton, SIGNAL(clicked()), this, SLOT(timer_run_once()));
    connect(ui->resetButton, SIGNAL(clicked()), this, SLOT(gui_reset()));

    connect(ui->screenButton, SIGNAL(clicked()), this, SLOT(disp_screen()));


    /*
     * connect clock's signals and slots
     */
    connect(ui->clockHorizontalSlider, SIGNAL(valueChanged(int)),
            this, SLOT(gui_clk_update(int)));
    connect(ui->clockHorizontalSlider, SIGNAL(valueChanged(int)),
            this, SLOT(clock_update(int)));
    connect(&clock, SIGNAL(timeout()), this, SLOT(timer_run_once()));


    /*
     * other initialization
     */
    screen = new QDialog(this);
    keyboard_screen = new keyboardTextEdit(screen);

    screen->resize(keyboard_screen->width(), keyboard_screen->height());
    screen->setMinimumSize(keyboard_screen->width(), keyboard_screen->height());
    screen->setMaximumSize(keyboard_screen->width(), keyboard_screen->height());

    screen->setWindowTitle(QString("Screen"));


    commd_model = NULL;
    reg_model = NULL;
    main_mem_model = NULL;
    static_mem_model = NULL;
    disp_mem_model = NULL;

    my_cpu = NULL;
    my_cpu_thread = NULL;

    codeview_current_row = 0;
    my_cpu_current_pc = 0;
    is_stopped = false;
    exec_result = false;
}

MainWindow::~MainWindow()
{
    delete_all();
    delete screen;
    delete ui;
}

void MainWindow::reset_all()
{
    if (clock.isActive())
        clock.stop();

    if (my_cpu)
    {
        if (my_cpu_thread && my_cpu_thread->isRunning())
            emit cpu_rst();
        else
        {
            my_cpu->rst();
        }
    }
}

void MainWindow::delete_all()
{
    reset_all();

    if (my_cpu_thread)
    {
        if (my_cpu_thread->isRunning())
            my_cpu_thread->terminate();
        if (my_cpu)
            delete my_cpu;
        delete my_cpu_thread;
    }
}

void MainWindow::gui_reset()
{
    timer_run_init();
    reset_all();
    other_setting_init();
}

void MainWindow::gui_ins_init(const dword* mem, const dword size)
{
    // deassemble
    deassembler my_deassembler;
    my_deassembler.load(mem, size, qtCPU::USER_MEM);
    //mydeassembler.print();
    //std::cout << "assembler: " << myassembler.get_commd_num() << std::endl;
    //std::cout << "deassembler: " << mydeassembler.get_instru_num() << std::endl;

    QStandardItemModel* model = new QStandardItemModel;
    model->setColumnCount(3);
    model->setHeaderData(0,Qt::Horizontal,tr("Address"));
    model->setHeaderData(1,Qt::Horizontal,tr("Instruction"));
    model->setHeaderData(2,Qt::Horizontal,tr("Command"));

    int i;
    QString temp;
    //printf("%d\n", mydeassembler.get_instru_num());
    for (i = 0; i < my_deassembler.get_instru_num(); i++) {

        model->setItem(i, 0, new QStandardItem( dword2QString( qtCPU::USER_MEM+i*4 ) ));
        model->item(i, 0)->setTextAlignment(Qt::AlignCenter);

        temp.sprintf("%s", my_deassembler.get_instru(i).c_str());
        model->setItem(i, 1, new QStandardItem(temp));
        model->item(i, 1)->setTextAlignment(Qt::AlignCenter);

        model->setItem(i, 2, new QStandardItem( dword2QString( mem[i] ) ));
        model->item(i, 2)->setTextAlignment(Qt::AlignCenter);
    }
    model->setItem(i, 0, new QStandardItem("End"));
    model->item(i, 0)->setTextAlignment(Qt::AlignCenter);

    model->setItem(i, 1, new QStandardItem("End"));
    model->item(i, 1)->setTextAlignment(Qt::AlignCenter);

    model->setItem(i, 2, new QStandardItem("End"));
    model->item(i, 2)->setTextAlignment(Qt::AlignCenter);

    if (ui->commdTableView->model())
        delete ui->commdTableView->model();

    commd_model = model;
    ui->commdTableView->setModel(commd_model);
    ui->commdTableView->resizeColumnToContents(0);
    ui->commdTableView->resizeColumnToContents(2);
    //ui->commdTableView->horizontalHeader()->setResizeMode(QHeaderView::Fixed);
    ui->commdTableView->horizontalHeader()->setResizeMode(1, QHeaderView::Stretch);
}

void MainWindow::gui_ins_counter_update(int ins_counter)
{
    if (ins_counter >= commd_model->rowCount()) return;

    gui_ins_paint_row(commd_model, codeview_current_row, QBrush(QColor(255, 255, 255)));
    codeview_current_row = (dword)ins_counter;
    gui_ins_paint_row(commd_model, codeview_current_row, QBrush(QColor(255, 0, 0, 127)));
}

int MainWindow::gui_ins_paint_row(QStandardItemModel *model, int row, const QBrush &brush) {
    if (!model) return 1;
    for (int i = 0; model->item(row, i); i++)
        model->item(row, i)->setBackground(brush);
    return 0;
}

void MainWindow::gui_reg_init(const dword reg[])
{
    QStandardItemModel* model = new QStandardItemModel;
    model->setColumnCount(2);
    model->setHeaderData(0, Qt::Horizontal, tr("Reg"));
    model->setHeaderData(1, Qt::Horizontal, tr("Content"));

    QString temp;
    char s[5];
    dword i;
    for (int j = 0; j < qtCPU::REGNUM; j++)
    {
        i = (j+1) % qtCPU::REGNUM;

        deassembler::regName(s, j);
        temp.sprintf("%s (%d)", s, j);
        model->setItem(i, 0, new QStandardItem(temp));
        model->item(i, 0)->setTextAlignment(Qt::AlignCenter);

        model->setItem(i, 1, new QStandardItem(dword2QString( reg[j] )));
        model->item(i, 1)->setTextAlignment(Qt::AlignCenter);
    }

    if (ui->regTableView->model())
        delete ui->regTableView->model();

    reg_model = model;
    ui->regTableView->setModel(reg_model);

    ui->regTableView->resizeColumnsToContents();
    //ui->regTableView->horizontalHeader()->setResizeMode(QHeaderView::Fixed);
    ui->regTableView->horizontalHeader()->setResizeMode(QHeaderView::Stretch);

}

void MainWindow::gui_reg_update(const dword reg, const dword val)
{
    if (reg > qtCPU::REGNUM)
    {
        for (int i = 0; i < qtCPU::REGNUM; i++)
            reg_model->item(i, 1)->setBackground(QBrush(QColor(255, 255, 255)));
    }
    else
    {
        for (dword j = 0; j < qtCPU::REGNUM; j++)
        {
            if (j == reg)
            {
                dword i = (j+1) % qtCPU::REGNUM;
                reg_model->item(i, 1)->setText( dword2QString( val ) );
                reg_model->item(i, 1)->setBackground(QBrush(QColor(0, 0, 255, 127)));
            }
        }

        /*
        * update if it's PC
        */
        if (reg == qtCPU::REG_PC)
        {
            my_cpu_current_pc = val;
            gui_ins_counter_update( (int)(val - qtCPU::USER_MEM) / 4 );
        }
    }
}

void MainWindow::gui_mem_init_view(
        QTableView* tableview, QStandardItemModel** view_model,
        dword addr_st, dword addr_ed, const byte* mem_ptr
        )
{
    QStandardItemModel* model = new QStandardItemModel;
    model->setColumnCount(3);
    model->setHeaderData(0, Qt::Horizontal, tr("Address"));
    model->setHeaderData(1, Qt::Horizontal, tr("Content"));
    model->setHeaderData(2, Qt::Horizontal, tr("Ascii"));

    for (int offset = 0; addr_st+offset < addr_ed; offset+=4) {
        QStringList str_list;
        int row = offset/4;

        /*
         * address
         */
        model->setItem(row, 0, new QStandardItem(dword2QString(addr_st+offset)));
        model->item(row, 0)->setTextAlignment(Qt::AlignCenter);

        /*
         * content
         */
        str_list.clear();
        for (int j = 0; j < 4; j++) {
            QString str;
            str.sprintf("%02X", mem_ptr[addr_st+offset+j]);
            str_list << str;
        }
        model->setItem(row, 1, new QStandardItem( str_list.join(" ") ));
        model->item(row, 1)->setTextAlignment(Qt::AlignCenter);

        /*
         * ascii
         */
        str_list.clear();
        for (int j = 0; j < 4; j++) {
            str_list << QString( (mem_ptr[addr_st+offset+j] > 31 && mem_ptr[addr_st+offset+j] < 127) ?
                           QChar(mem_ptr[addr_st+offset+j]) : QChar('.') );
        }
        model->setItem(row, 2, new QStandardItem( str_list.join(" ") ));
        model->item(row, 2)->setTextAlignment(Qt::AlignCenter);
    }

    if (tableview->model())
        delete tableview->model();

    *view_model = model;
    tableview->setModel(*view_model);
    tableview->setColumnWidth(0, 70);
    tableview->setColumnWidth(1, 85);
    tableview->setColumnWidth(2, 85);
    //tableview->horizontalHeader()->setResizeMode(QHeaderView::Fixed);
    //tableview->horizontalHeader()->setResizeMode(QHeaderView::Stretch);
}

void MainWindow::gui_mem_init(const byte* mem_ptr)
{
    gui_mem_init_view(
                ui->static_memTableView, &static_mem_model,
                qtCPU::STATIC_MEM, qtCPU::MAIN_MEM, mem_ptr
                );

    gui_mem_init_view(
                ui->main_memTableView, &main_mem_model,
                qtCPU::MAIN_MEM, qtCPU::DISP_MEM, mem_ptr
                );

    gui_mem_init_view(
                ui->disp_memTableView, &disp_mem_model,
                qtCPU::DISP_MEM, qtCPU::END_MEM, mem_ptr
                );
}

void MainWindow::gui_mem_update_view(
        QStandardItemModel **view_model, const dword addr_st, const dword addr, const dword val
        )
{
    QStringList str_list;
    QString str;
    dword row, col;
    byte value;
    QString temp;

    for (dword i = 0; i < 4; i++)
    {
        row = (addr+i - addr_st) / 4, col = (addr+i) % 4;
        if (row >= (*view_model)->rowCount()) break;
        value = (val >> (8*(3-i))) & 0xFF;


        str = QString((*view_model)->item(row, 1)->text());
        str_list = QString((*view_model)->item(row, 1)->text()).split(" ");

        temp.sprintf("%02X", value);
        str_list[col] = temp;

        (*view_model)->setItem(row, 1, new QStandardItem( str_list.join(" ") ));
        (*view_model)->item(row, 1)->setTextAlignment(Qt::AlignCenter);


        str = QString((*view_model)->item(row, 2)->text());
        str[col*2] = (value > 31 && value < 127) ? QChar(value) : QChar('.');

        (*view_model)->setItem(row, 2, new QStandardItem( str ));
        (*view_model)->item(row, 2)->setTextAlignment(Qt::AlignCenter);
    }
}

void MainWindow::gui_mem_update(const dword mem_addr, const dword val)
{
    if (qtCPU::USER_MEM <= mem_addr && mem_addr < qtCPU::STATIC_MEM)
    {

    }
    else if (qtCPU::STATIC_MEM <= mem_addr && mem_addr < qtCPU::MAIN_MEM)
    {
        gui_mem_update_view(&static_mem_model, qtCPU::STATIC_MEM, mem_addr, val);
    }
    else if (qtCPU::MAIN_MEM <= mem_addr && mem_addr < qtCPU::DISP_MEM)
    {
        gui_mem_update_view(&main_mem_model, qtCPU::MAIN_MEM, mem_addr, val);
    }
    else if (qtCPU::DISP_MEM <= mem_addr && mem_addr < qtCPU::END_MEM)
    {
        gui_mem_update_view(&disp_mem_model, qtCPU::DISP_MEM, mem_addr, val);

        emit disp_fresh(mem_addr, val);
    }
}

void MainWindow::gui_clk_update(int val)
{
    int speed = calc_clock_cycle(val);
    QString str;
    str.sprintf("%s%d%s", "Speed: ", speed, " Hz");
    ui->clockLabel->setText( str );
}

int MainWindow::calc_clock_cycle(int val)
{
    if (val <= 20)
        return val;
    else if (val >= 99)
        return 1000;
    else
        return 20+(val-20)*12;
}

void MainWindow::gui_button_init()
{
    ui->step_runButton->setEnabled(true);
    ui->timer_runButton->setEnabled(true);
    ui->timer_stopButton->setEnabled(false);
    ui->resetButton->setEnabled(true);
    ui->screenButton->setEnabled(true);

    ui->clockHorizontalSlider->setEnabled(true);
    ui->clockLabel->setEnabled(true);

    ui->breakpointLineEdit->setEnabled(true);
}


void MainWindow::gui_button_timer_start()
{
    ui->step_runButton->setEnabled(false);
    ui->timer_runButton->setEnabled(false);
    ui->timer_stopButton->setEnabled(true);
    ui->resetButton->setEnabled(false);
    ui->screenButton->setEnabled(true);

    ui->clockHorizontalSlider->setEnabled(true);
    ui->clockLabel->setEnabled(true);

    ui->breakpointLineEdit->setEnabled(false);
}

void MainWindow::connect_init()
{
    /*
     * connect cpu's signals and slots
     */
    connect(this, SIGNAL(cpu_rst()),
            my_cpu, SLOT(rst()), Qt::QueuedConnection);

    connect(this, SIGNAL(cpu_run_once(int)),
            my_cpu, SLOT(pc_increment(int)), Qt::QueuedConnection);

    connect(my_cpu, SIGNAL(reg_update(dword,dword)),
            this, SLOT(gui_reg_update(dword,dword)), Qt::QueuedConnection);

    connect(my_cpu, SIGNAL(mem_update(dword,dword)),
            this, SLOT(gui_mem_update(dword,dword)), Qt::QueuedConnection);

    connect(my_cpu, SIGNAL(exec_result_send(bool)),
            this, SLOT(exec_result_receive(bool)), Qt::QueuedConnection);

    /*
     * connect screen
     */
    connect(this, SIGNAL(disp_fresh(dword,dword)),
            keyboard_screen, SLOT(fresh(dword,dword)), Qt::QueuedConnection);

    connect(keyboard_screen, SIGNAL(send_scancode(byte)),
            my_cpu, SLOT(set_keyboard_irq(byte)), Qt::QueuedConnection);
}

void MainWindow::other_setting_init()
{
    ins_counter = 0;

    gui_ins_counter_update(0);
    my_cpu_current_pc = qtCPU::USER_MEM;
    exec_result = true;
}

void MainWindow::about_to_open_file()
{
    qtCPU* temp_cpu = new qtCPU;

    int failed = open_file(temp_cpu);

    if (!failed)
    {
        delete_all();

        my_cpu = temp_cpu;
        timer_run_init();

        connect_init();
        other_setting_init();

        /*
         * init screen
         */
        keyboard_screen->init(QChar(' '));

        /*
         * move cpu to second thread
         */
        my_cpu_thread = new QThread(this);
        my_cpu->moveToThread(my_cpu_thread);
        my_cpu_thread->start(QThread::TimeCriticalPriority);

        gui_button_init();

        //QMessageBox::warning(this, tr("Congratulations!"), tr("Loading successfully!"), QMessageBox::Yes);
    }
    else
        delete temp_cpu;
}

int MainWindow::open_file(qtCPU* temp_cpu) {
    /*
     * load user's file
     */
    QString user_file =
            QFileDialog::getOpenFileName(
                this, tr("Open MIPS command file"), "*.s",
                tr("MIPS command file (*.s)")
                );

    if (user_file.isEmpty())
    {
        //QMessageBox::warning(this, tr("Warning"), tr("Command file doesn't exsist!"), QMessageBox::Yes);
        return 1;
    }

    if (load_user(user_file, temp_cpu))
    {
        return 1;
    }


    /*
     * load kernel
     */
    QString kernel_file =
            QFileDialog::getOpenFileName(
                this, tr("Open MIPS kernel file"), "*.s",
                tr("MIPS command file (*.s)")
                );

    if (kernel_file.isEmpty())
    {
        //QMessageBox::warning(this, tr("Warning"), tr("Kernel file doesn't exsist!"), QMessageBox::Yes);
        return 1;
    }

    if (load_kernel(kernel_file, temp_cpu))
    {
        return 1;
    }

    return 0;
}

int MainWindow::load_kernel(QString fileName, qtCPU* my_cpu)
{
    assembler my_assembler;
    if (my_assembler.load(fileName.toStdString()))
    {
        QMessageBox::warning(
                    this, tr("Warning"),
                    tr("Assembler loaded kernel unsuccessfully!"), QMessageBox::Yes
                    );
        return 1;
    }

    if (my_assembler.assemble_kernel())
    {
        QMessageBox::warning(
                    this, tr("Warning"), tr("Assemble kernel unsuccessfully!"), QMessageBox::Yes
                    );
        return 1;
    }
    //myassembler1.print();

    dword* mem = new dword[my_assembler.get_commd_num()];
    my_assembler.save(mem);

    int failed =
            my_cpu->load_mem_commd_data(mem, my_assembler.get_commd_num(), qtCPU::KERNEL_MEM, qtCPU::USER_MEM);
    delete [] mem;
    if (failed)
    {
        QMessageBox::warning(this, tr("Warning"), tr("Memory is too small, cant'load MIPS Command error!"), QMessageBox::Yes);
        return 1;
    }
    else
        return 0;
}

int MainWindow::load_user(QString fileName, qtCPU* my_cpu)
{
    int failed = 0;

    /*
     * assemble
     */
    assembler my_assembler;

    failed = my_assembler.load(fileName.toStdString());
    if (failed)
    {
        QMessageBox::warning(
                    this, tr("Warning"),
                    tr("Assembler loaded MIPS command unsuccessfully!"), QMessageBox::Yes
                    );
        return 1;
    }

    failed = my_assembler.assemble();
    if (failed)
    {
        QMessageBox::warning(
                    this, tr("Warning"),
                    tr("Assemble MIPS command unsuccessfully!"), QMessageBox::Yes
                    );
        return 1;
    }

    dword* mem = new dword[my_assembler.get_commd_num()];
    failed = my_assembler.save(mem);
    if (failed)
    {
        return 1;
    }

    /*
     * CPU load commd
     */
    failed =
            my_cpu->load_mem_commd_data(mem, my_assembler.get_commd_num(), qtCPU::USER_MEM, qtCPU::STATIC_MEM);
    if (failed)
    {
        QMessageBox::warning(this, tr("Warning"), tr("Memory is too small, cant'load MIPS Command error!"), QMessageBox::Yes);
        return 1;
    }

    /*
     * CPU load static data
     */
    byte* static_mem = new byte[my_assembler.get_static_mem_size()];
    my_assembler.save_static_mem(static_mem);
    failed = my_cpu->load_static_data(static_mem, my_assembler.get_static_mem_size());
    delete [] static_mem;
    if (failed)
    {
        QMessageBox::warning(
                    this, tr("Warning"),
                    tr("CPU loaded MIPS command unsuccessfully!"), QMessageBox::Yes
                    );
        return 1;
    }

    /*
     * memory GUI init
     */
    gui_mem_init(my_cpu->get_mem_ptr());

    /*
     * command GUI init
     */
    gui_ins_init(mem, my_assembler.get_commd_num());
    delete mem;

    /*
     * reg GUI init
     */
    gui_reg_init(my_cpu->get_reg_ptr());

    /*
     * break point init
     */
    ui->breakpointLineEdit->setText( dword2QString( qtCPU::USER_MEM + 4*my_assembler.get_commd_num() ) );

    return 0;
}

void MainWindow::exec_result_receive(bool flag)
{
    //exec_result_mutex.lock();
    exec_result = flag;
    //reg_update_flag = 0;
    //exec_result_mutex.lock();
}

dword MainWindow::get_break_point()
{
    return ui->breakpointLineEdit->text().toInt(0, 16);
}

void MainWindow::clock_update(int val)
{
    int speed = calc_clock_cycle(val);
    clock.setInterval( 1000/speed );
}

void MainWindow::timer_run_init()
{
    gui_clk_update( ui->clockHorizontalSlider->value() );
    clock_update( ui->clockHorizontalSlider->value() );
    clock.stop();
    is_stopped = false;
}

void MainWindow::timer_run_restart()
{
    clock.start();

    gui_button_timer_start();
}

void MainWindow::timer_run_stop()
{
    clock.stop();

    gui_button_init();
}

void MainWindow::timer_run_once()
{
    //exec_result_mutex.lock();

    if (!is_stopped && get_break_point() == my_cpu_current_pc)
    {
        is_stopped = true;
        timer_run_stop();
    }
    else if (exec_result)
    {
        is_stopped = false;
        //exec_result = false;
        gui_reg_update(qtCPU::REGNUM+1, 0);

        emit cpu_run_once(0);
        ins_counter++;
        qDebug() << ins_counter;
    }
    else
    {
        QMessageBox::warning(
                    this, tr("Warning"),
                    tr("CPU can't continue working!"), QMessageBox::Yes
                    );
        timer_run_stop();
    }

    //exec_result_mutex.unlock();
}

void MainWindow::disp_screen()
{
    if (screen->isHidden())
        screen->show();
    else
        screen->hide();
}

QString MainWindow::dword2QString(const dword &data) {
    QString str;
    str.sprintf("%08X", data);
    return str;
}

void MainWindow::about() {
    QMessageBox::about(this, tr("About MIPS Virtual Machine"),
                       tr("<h2>MIPS Virtual Machine v1.0</h2>"
                          "<p>Copyright &copy; 2013 WJR<p>"
                          "MIPS Virtual Machine is a software implemented abstraction of the underlying MIPS hardware,"
                          "It emulates the MIPS-based machine and implement many basic instrution and psedo-instrution."));
}
