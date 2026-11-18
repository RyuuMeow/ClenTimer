#include <QtWidgets>
#include <memory>
#include <qt_windows.h>
#include <mmsystem.h>

enum class Mode { Setup, Running, Paused, Finished };

class TimerWindow final : public QWidget {
    Mode mode = Mode::Setup;
    std::unique_ptr<QSettings> prefs = QFileInfo::exists(QCoreApplication::applicationDirPath()+"/portable.mode")
        ? std::make_unique<QSettings>(QCoreApplication::applicationDirPath()+"/ClenTimer.ini", QSettings::IniFormat)
        : std::make_unique<QSettings>("OrbitTimer","Orbit Timer"); // Keep existing installed preferences.
    QPushButton *repeat;
    QLineEdit *minutes;
    int wheelRemainder = 0;
    QLabel *readout, *unit;
    QPushButton *less, *more, *action, *start, *end, *minimize, *closeButton;
    QTimer tick, blink, chime;
    QElapsedTimer elapsed;
    qint64 total = 0, bank = 0;
    int soundCount = 0;
    bool dim = false;
    QString lastDigits;
    QVariantAnimation morph;
    double compact = 0;
    QList<QWidget *> setupWidgets, clockWidgets;

    static constexpr int wide = 412, square = 174, canvas = 432;
    bool animating = false;
    QPixmap setupLayer, clockLayer;

    void showPage(bool clock) {
        for (auto widget : setupWidgets) widget->setVisible(!clock);
        for (auto widget : clockWidgets) widget->setVisible(clock);
    }
    QPixmap snapshot(const QList<QWidget *> &widgets) {
        const qreal scale = devicePixelRatioF();
        QPixmap layer(qRound(canvas*scale),qRound(square*scale));
        layer.setDevicePixelRatio(scale);
        layer.fill(Qt::transparent);
        QPainter painter(&layer);
        for (auto widget : widgets)
            widget->render(&painter,widget->pos(),QRegion(),QWidget::DrawChildren);
        return layer;
    }
    void transition(bool shrink) {
        setupLayer = snapshot(setupWidgets);
        clockLayer = snapshot(clockWidgets);
        for (auto widget : setupWidgets + clockWidgets) widget->hide();
        animating = true;
        compact = shrink ? 0 : 1;
        morph.setStartValue(compact);
        morph.setEndValue(shrink ? 1.0 : 0.0);
        morph.start();
    }
    void finishTransition() {
        bool clock = mode != Mode::Setup;
        compact = clock ? 1 : 0;
        animating = false;
        showPage(clock);
        setupLayer = {}; clockLayer = {};
        update();
    }

    QLabel *label(const QString &s, int x, int y, int w, int h) {
        auto l = new QLabel(s, this);
        l->setGeometry(x,y,w,h);
        l->setAlignment(Qt::AlignCenter);
        return l;
    }
    void face(QPushButton *b, QString glyph, QString tip) {
        b->setIcon(QIcon(QCoreApplication::applicationDirPath()+"/icons/"+glyph+".svg"));
        b->setAccessibleName(tip);
    }
    QPushButton *button(QString glyph, QString tip, int x, int y, int w, int h) {
        auto b = new QPushButton(this);
        b->setGeometry(x,y,w,h);
        b->setIconSize(QSize(18,18));
        b->setCursor(Qt::PointingHandCursor);
        face(b,glyph,tip);
        return b;
    }
    double value() const { return minutes->hasAcceptableInput() ? minutes->text().toDouble() : 0; }
    void bump(int direction) {
        if (value() < 1 || (value() == 1 && direction < 0)) {
            minutes->setText(direction > 0 ? "1" : "0.5");
            return;
        }
        int v = int(value());
        int band = v - (direction < 0);
        int step = band < 10 ? 1 : band < 30 ? 5 : 10;
        int limit = direction > 0 ? (v < 10 ? 10 : v < 30 ? 30 : 99)
                                  : (v > 30 ? 30 : v > 10 ? 10 : 1);
        int next = direction > 0 ? qMin(limit,v+step) : qMax(limit,v-step);
        minutes->setText(QString::number(next));
    }
    qint64 remaining() const {
        return mode == Mode::Running ? qMax<qint64>(0, bank-elapsed.elapsed()) : bank;
    }
    void display() {
        int seconds = int((remaining()+999)/1000);
        QString s = QString("%1:%2").arg(seconds/60,2,10,QChar('0')).arg(seconds%60,2,10,QChar('0'));
        if (s != lastDigits) { readout->setText(s); lastDigits = s; }
        update();
    }
    void enter(Mode next) {
        const Mode previous = mode;
        if (previous == Mode::Running && next == Mode::Setup) bank = remaining();
        mode = next;
        tick.stop(); blink.stop(); chime.stop(); dim = false;
        if (next != Mode::Setup) readout->setStyleSheet("");
        switch (next) {
        case Mode::Setup:
            if (previous != Mode::Setup) transition(false);
            break;
        case Mode::Running:
            if (previous == Mode::Setup) {
                if (!clockWidgets.contains(end)) clockWidgets.append(end);
                action->setFixedWidth(84);
            }
            elapsed.restart(); tick.start();
            face(action,"pause-fill","暫停");
            display();
            if (previous == Mode::Setup) transition(true);
            break;
        case Mode::Paused:
            face(action,"play-fill","繼續"); break;
        case Mode::Finished:
            if (repeat->isChecked()) {
                face(action,"play-fill","立即開始下一輪");
            } else {
            end->hide();
            clockWidgets.removeOne(end);
            action->setFixedWidth(124);
            face(action,"check","完成");
            }
            readout->setStyleSheet("color:#ff6060"); blink.start();
            playChime(); soundCount = 1; chime.start(); break;
        }
        start->setEnabled(value()>0);
        display();
    }
    void playChime() {
        const auto file = QCoreApplication::applicationDirPath()+"/finish.wav";
        PlaySoundW(reinterpret_cast<LPCWSTR>(file.utf16()),nullptr,SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
    }
    void activate() {
        if (morph.state()==QAbstractAnimation::Running) return;
        switch (mode) {
        case Mode::Setup:
            if (!value()) return;
            total = bank = value()*60000LL;
            enter(Mode::Running); break;
        case Mode::Running:
            bank = remaining();
            enter(bank ? Mode::Paused : Mode::Finished); break;
        case Mode::Paused: enter(Mode::Running); break;
        case Mode::Finished:
            if (repeat->isChecked()) { bank = total; enter(Mode::Running); }
            else enter(Mode::Setup);
            break;
        }
    }
protected:
    bool eventFilter(QObject *object, QEvent *event) override {
        if ((object == minimize || object == closeButton) &&
            (event->type() == QEvent::Enter || event->type() == QEvent::Leave)) {
            const QString glyph = object == minimize ? "minus" : "x";
            const QString suffix = event->type() == QEvent::Enter ? ".svg" : "-dim.svg";
            static_cast<QPushButton *>(object)->setIcon(QIcon(QCoreApplication::applicationDirPath()+"/icons/"+glyph+suffix));
        }
        if (object == minutes) {
            if (event->type() == QEvent::Leave || event->type() == QEvent::Hide)
                wheelRemainder = 0;
            if (event->type() == QEvent::Wheel && mode == Mode::Setup) {
                auto *wheel = static_cast<QWheelEvent *>(event);
                const int delta = wheel->angleDelta().y();
                if ((delta > 0 && wheelRemainder < 0) || (delta < 0 && wheelRemainder > 0))
                    wheelRemainder = 0;
                wheelRemainder += delta;
                const int steps = wheelRemainder / QWheelEvent::DefaultDeltasPerStep;
                wheelRemainder %= QWheelEvent::DefaultDeltasPerStep;
                if (steps)
                    minutes->setText(QString::number((qBound(0, int(value()) + steps, 99) == 0 ? 0.5 : qBound(1, int(value()) + steps, 99))));
                wheel->accept();
                return true;
            }
        }
        return QWidget::eventFilter(object,event);
    }
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.fillRect(rect(),Qt::transparent);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        p.setRenderHint(QPainter::Antialiasing);
        const double w = wide - (wide-square)*compact;
        const double left = (width()-w)/2;
        p.setPen(QPen(QColor("#484848"),1));
        p.setBrush(QColor("#171717"));
        p.drawRoundedRect(QRectF(left+1,1,w-2,height()-2),18,18);
        const QRectF r(left+6,6,w-12,height()-12);
        QPainterPath rim;
        const double d=26, a=r.left(), b=r.top(), c=r.right(), f=r.bottom();
        // The visible prefix shrinks, so draw counterclockwise for clockwise depletion.
        rim.moveTo(r.center().x(),b); rim.lineTo(a+d/2,b);
        rim.arcTo(a,b,d,d,90,90); rim.lineTo(a,f-d/2);
        rim.arcTo(a,f-d,d,d,180,90); rim.lineTo(c-d/2,f);
        rim.arcTo(c-d,f-d,d,d,270,90); rim.lineTo(c,b+d/2);
        rim.arcTo(c-d,b,d,d,0,90); rim.closeSubpath();
        const double alpha = qBound(0.0,compact,1.0);
        if (alpha > 0 && total > 0) {
            p.setOpacity(alpha);
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor("#353535"),2.5));
            p.drawPath(rim);
            double fraction = double(remaining())/total;

            QPen progress(QColor("#eeeeec"),2.5,Qt::SolidLine,Qt::RoundCap);
            if (fraction < 1) progress.setDashPattern({qMax(0.001,fraction*rim.length()/2.5),rim.length()});
            p.setPen(progress);
            if (fraction > 0) p.drawPath(rim);
        }
        if (animating) {
            QPainterPath clip;
            clip.addRoundedRect(QRectF(left+2,2,w-4,height()-4),17,17);
            p.setClipPath(clip);
            const QPointF origin(0,0);
            p.setOpacity(qMax(0.0,1-2*alpha));
            p.drawPixmap(origin,setupLayer);
            p.setOpacity(qMax(0.0,2*alpha-1));
            p.drawPixmap(origin,clockLayer);
        }
    }
    void mousePressEvent(QMouseEvent *e) override {
        if(morph.state()!=QAbstractAnimation::Running && e->button()==Qt::LeftButton) windowHandle()->startSystemMove();
    }
public:
    TimerWindow() {
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowMinimizeButtonHint | Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_TranslucentBackground);
        setWindowTitle("ClenTimer");
        setFixedSize(canvas,square);
        setStyleSheet(R"(
          QWidget {color:#f3f3f1;}
          QLabel {background:transparent;}
          QPushButton {background:transparent;border:0;border-radius:8px;}
          QPushButton:hover {background:#363636;}
          QPushButton#go, QPushButton#end {border-radius:13px;}
          QPushButton#end {background:#925a5a;}
          QPushButton#end:hover {background:#a16767;}
          QPushButton#windowControl:hover {background:transparent;}
          QPushButton#repeat {background:#777;border-radius:13px;}
          QPushButton#repeat:checked {background:#f5f5f5;}
          QPushButton#step {background:#282828;border:1px solid #393939;border-radius:10px;}
          QPushButton#step:hover {background:#404040;}
          QPushButton#go {background:#ededeb;color:#141414;font-weight:600;}
          QPushButton#go:hover {background:white;}
          QPushButton#go:disabled {background:#444;color:#999;}
          QLineEdit {background:#202020;border:1px solid #343434;border-radius:12px;selection-background-color:#777;}
          QLineEdit:focus {border-color:#898989;}
        )");
        minimize=button("minus-dim","最小化",334,18,29,25);
        closeButton=button("x-dim","關閉",365,18,29,25);
        for (auto button : {minimize,closeButton}) {
            button->setObjectName("windowControl");
            button->installEventFilter(this);
        }
        unit=label("MIN",106,33,200,17);
        unit->setStyleSheet("color:#aaa;font:600 9px 'Segoe UI'");
        minutes=new QLineEdit(this);
        minutes->installEventFilter(this);
        minutes->setAccessibleName("分鐘");
        minutes->setGeometry(144,53,124,55);
        minutes->setAlignment(Qt::AlignCenter);
        minutes->setMaxLength(3);
        minutes->setValidator(new QRegularExpressionValidator(QRegularExpression(R"(0\.5|[1-9][0-9]?)"),this));
        minutes->setFont(QFont("Consolas",29,QFont::Bold));
        minutes->setText(QString::number(qBound(0.5,prefs->value("minutes",25).toDouble(),99.0)));
        readout=label("",66,50,280,61);
        readout->setFont(QFont("Consolas",35,QFont::Bold));
        less=button("caret-left","減少分鐘",91,62,36,36);
        more=button("caret-right","增加分鐘",285,62,36,36);
        for(auto b:{less,more}) b->setObjectName("step");
        repeat=button("repeat","循環計時",101,122,34,34);
        repeat->setObjectName("repeat");
        repeat->setCheckable(true);
        start=button("play-fill","開始",144,122,124,34);
        start->setObjectName("go");
        connect(start,&QPushButton::clicked,this,[this]{activate();});
        action=button("pause-fill","暫停",144,122,84,34);
        end=button("end","結束計時",234,122,34,34);
        end->setObjectName("end");
        connect(end,&QPushButton::clicked,this,[this]{if (morph.state()!=QAbstractAnimation::Running) enter(Mode::Setup);});
        action->setObjectName("go");
        connect(minutes,&QLineEdit::textChanged,this,[this] {
            start->setEnabled(value()>0);
            if(value()>0) prefs->setValue("minutes",value());
        });
        connect(minutes,&QLineEdit::returnPressed,this,[this]{activate();});
        connect(less,&QPushButton::clicked,this,[this]{bump(-1);});
        connect(more,&QPushButton::clicked,this,[this]{bump(1);});
        connect(action,&QPushButton::clicked,this,[this]{activate();});
        connect(minimize,&QPushButton::clicked,this,&QWidget::showMinimized);
        connect(closeButton,&QPushButton::clicked,this,&QWidget::close);
        setupWidgets = {repeat, minutes, unit, less, more, start, minimize, closeButton};
        clockWidgets = {readout, action, end};
        QEasingCurve ease(QEasingCurve::OutBack);
        ease.setOvershoot(0.60);
        morph.setEasingCurve(ease);
        morph.setDuration(460);
        connect(&morph,&QVariantAnimation::valueChanged,this,[this](const QVariant &v){
            compact=v.toDouble(); update();
        });
        connect(&morph,&QVariantAnimation::finished,this,&TimerWindow::finishTransition);
        for (auto widget : setupWidgets)
            widget->move(widget->pos()+QPoint((canvas-wide)/2,0));
        action->move(canvas/2-62,122);
        end->move(canvas/2+28,122);
        readout->setGeometry(canvas/2-83,35,166,73);
        showPage(false);
        tick.setInterval(100); blink.setInterval(550); chime.setInterval(820);
        connect(&tick,&QTimer::timeout,this,[this] {
            if(!remaining()) {bank=0; enter(Mode::Finished);} else display();
        });
        connect(&blink,&QTimer::timeout,this,[this]{
            dim=!dim; readout->setStyleSheet(dim?"color:#773636":"color:#ff6060");
        });
        connect(&chime,&QTimer::timeout,this,[this]{
            if (soundCount < 3) {
                playChime();
                ++soundCount;
            } else if (repeat->isChecked()) {
                bank = total;
                enter(Mode::Running);
            } else {
                chime.stop();
            }
        });
        setFocusPolicy(Qt::StrongFocus);
        enter(Mode::Setup); setFocus();
    }
};

int main(int argc,char **argv) {
    QApplication app(argc,argv);
    app.setOrganizationName("ClenTimer");
    app.setApplicationName("ClenTimer");
    app.setWindowIcon(QIcon(app.applicationDirPath()+"/ClenTimer.png"));
    TimerWindow w; w.show();
    return app.exec();
}
