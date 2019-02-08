# You may have to set this: QTDIR=/usr/lib/qt4

CPPFLAGS=-I/usr/include/qt5 -I/usr/include/qt5/QtCore -I/usr/include/qt5/QtGui -I/usr/include/qt5/QtWidgets
CXXFLAGS=-O2 -fomit-frame-pointer
LDFLAGS=-s

all: consolehelper consolehelper-qt

clean:
	@rm -f *.o consolehelper consolehelper-qt *.moc

consolehelper: consolehelper-nox.o
	$(CC) $(CXXFLAGS) $(LDFLAGS) -o $@ -lpam -lpam_misc $< -lsupc++
	chmod 4755 consolehelper

consolehelper-qt: consolehelper.o consolehelperdlg.o
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) -o $@ -lQt5Core -lQt5Gui -lQt5Widgets -lpam -lpam_misc consolehelper.o consolehelperdlg.o
	chmod 4755 consolehelper-qt

consolehelper.o: consolehelper.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ -c $<

consolehelper-nox.o: consolehelper.cpp
	$(CC) $(CPPFLAGS) $(CXXFLAGS) -DDISABLE_X11 -o $@ -c $<

consolehelperdlg.o: consolehelperdlg.cpp consolehelperdlg.moc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ -c consolehelperdlg.cpp

consolehelperdlg.moc: consolehelperdlg.h
	moc -o $@ $<

install: consolehelper consolehelper-qt
	mkdir -p $(DESTDIR)/usr/bin
	cp -a consolehelper $(DESTDIR)/usr/bin
	cp -a consolehelper-qt $(DESTDIR)/usr/bin
