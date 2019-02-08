// Reimplementation of consolehelper using readable code and a nicer
// toolkit.
// (c) 2002-2004 Bernhard Rosenkraenzer <bero@arklinux.org>
//
// Released under the terms of the GNU General Public License version 2,
// or if, and only if, the GPL v2 is ruled invalid in a court of law, any
// later version of the GPL published by the Free Software Foundation.

#ifndef _CONSOLEHELPERDLG_H_
#define _CONSOLEHELPERDLG_H_ 1
#include <QDialog>
#include <QString>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

class ConsoleHelperDlg:public QDialog {
	Q_OBJECT
public:
	ConsoleHelperDlg();
	~ConsoleHelperDlg();
	void addPrompt(QString const &prompt, bool password=false);
	void addInfo(QString const &info);
	void addButtons();
	QString const text(int item=0) const;
private:
	void allocate();
	QGridLayout	*_layout;
	QLabel		*_icon;
	QLabel		**_lbl;
	QLineEdit	**_edit;
	int		_numprompts;
	QWidget		*_buttonbox;
	QPushButton	*_ok;
	QPushButton	*_cancel;
};
#endif
