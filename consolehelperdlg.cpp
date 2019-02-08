// Reimplementation of consolehelper using readable code and a nicer
// toolkit.
// (c) 2002-2004 Bernhard Rosenkraenzer <bero@arklinux.org>
//
// Released under the terms of the GNU General Public License version 2,
// or if, and only if, the GPL v2 is ruled invalid in a court of law, any
// later version of the GPL published by the Free Software Foundation.

#include "consolehelperdlg.moc"
#include <QPixmap>
#include <cstdlib>

ConsoleHelperDlg::ConsoleHelperDlg():QDialog(0), _numprompts(0)
{
	_layout=new QGridLayout(this);
	_layout->setMargin(10);
	_layout->setSpacing(5);
}

ConsoleHelperDlg::~ConsoleHelperDlg()
{
	free(_lbl);
	free(_edit);
}

void ConsoleHelperDlg::addPrompt(QString const &prompt, bool password)
{
	allocate();
	_lbl[_numprompts]=new QLabel("&"+prompt, this);
	_edit[_numprompts]=new QLineEdit(this);
	if(password)
		_edit[_numprompts]->setEchoMode(QLineEdit::Password);
	_layout->addWidget(_lbl[_numprompts], _numprompts, 1);
	_layout->addWidget(_edit[_numprompts], _numprompts, 2);
	_lbl[_numprompts]->setBuddy(_edit[_numprompts]);
	_numprompts++;
}

void ConsoleHelperDlg::addInfo(QString const &info)
{
	allocate();
	_lbl[_numprompts]=new QLabel(info, this);
	_edit[_numprompts]=0;
	_layout->addWidget(_lbl[_numprompts], _numprompts, 1, 1, 2);
	_numprompts++;
}

void ConsoleHelperDlg::addButtons()
{
	// We have to add the icon here because this is where we know how many
	// prompts we have...
	QPixmap iconPic("/usr/share/icons/breeze/64/system-users.svg");
	_icon=new QLabel(this);
	_icon->setPixmap(iconPic);
	_layout->addWidget(_icon, 0, 0, _numprompts, 1);

	_buttonbox=new QWidget(this);
	QHBoxLayout *bbLayout=new QHBoxLayout(_buttonbox);
	_ok=new QPushButton("&OK", _buttonbox);
	bbLayout->addWidget(_ok);
	_cancel=new QPushButton("&Cancel", _buttonbox);
	bbLayout->addWidget(_cancel);
	_layout->addWidget(_buttonbox, _numprompts+1, 0, 1, 3);
	connect(_ok, SIGNAL(clicked()), this, SLOT(accept()));
	connect(_cancel, SIGNAL(clicked()), this, SLOT(reject()));
}

QString const ConsoleHelperDlg::text(int item) const
{
	if(item>=_numprompts || !_edit[item])
		return QString::null;
	return _edit[item]->text();
}

void ConsoleHelperDlg::allocate()
{
	if(_numprompts==0) {
		_lbl=(QLabel**)malloc(10*sizeof(QLabel*));
		_edit=(QLineEdit**)malloc(10*sizeof(QLineEdit*));
	} else if((_numprompts%10)==0) {
		_lbl=(QLabel**)realloc(_lbl, (_numprompts+10)*sizeof(QLabel*));
		_edit=(QLineEdit**)realloc(_edit, (_numprompts+10)*sizeof(QLineEdit*));
	}
}
