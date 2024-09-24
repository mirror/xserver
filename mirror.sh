#!/bin/bash
# Repo list

# import
source git_login.sh
# source git_mirror.sh

args=$@

function git_mirror() {
	bash git_mirror.sh $@ $args
}

# test
# git_mirror astyle svn https://svn.code.sf.net/p/astyle/code/trunk; exit

# repos
git_mirror astyle svn https://svn.code.sf.net/p/astyle/code/trunk
git_mirror busybox git git://busybox.net/busybox.git
git_mirror chere git git://repo.or.cz/chere.git
git_mirror chromium git https://chromium.googlesource.com/chromium/src.git
git_mirror comical svn svn://svn.code.sf.net/p/comical/code/trunk
git_mirror console-devel hg http://hg.code.sf.net/p/console-devel/code
git_mirror cvs-fast-export git git://gitorious.org/cvs-fast-export/cvs-fast-export.git
git_mirror cvsps git git://gitorious.org/cvsps/cvsps.git
git_mirror daphne-emu svn https://www.daphne-emu.com:9443/daphnesvn/branches/v_1_0
git_mirror darwinbuild svn http://svn.macosforge.org/repository/darwinbuild/trunk
git_mirror dd-wrt svn svn://svn.dd-wrt.com/DD-WRT
git_mirror desmume svn https://svn.code.sf.net/p/desmume/code/trunk
git_mirror dmidecode git http://git.savannah.gnu.org/r/dmidecode.git
git_mirror env-man git git://env-man.git.sourceforge.net/gitroot/env-man/env-man
git_mirror equalizerapo svn svn://svn.code.sf.net/p/equalizerapo/code/trunk
git_mirror firmware-mod-kit svn http://firmware-mod-kit.googlecode.com/svn/trunk
git_mirror freedownload svn svn://svn.code.sf.net/p/freedownload/code/trunc
git_mirror hydrairc svn http://svn.hydrairc.com/hydrairc/trunk
git_mirror jdownloader svn svn://svn.jdownloader.org/jdownloader/trunk
git_mirror launch4j git git://git.code.sf.net/p/launch4j/git
git_mirror levelzap git https://git01.codeplex.com/levelzap
git_mirror libdvdread git git://git.videolan.org/libdvdread.git
git_mirror libdvdnav git git://git.videolan.org/libdvdnav.git
git_mirror libosinfo git http://git.fedorahosted.org/git/libosinfo.git
git_mirror libX11 git git://anongit.freedesktop.org/xorg/lib/libX11
git_mirror libXi git git://anongit.freedesktop.org/xorg/lib/libXi
git_mirror listfix svn https://svn.code.sf.net/p/listfix/code/dev
git_mirror make git git://git.savannah.gnu.org/make
git_mirror mingw-org-wsl git git://git.code.sf.net/p/mingw/mingw-org-wsl
git_mirror mingw-w64 git git://git.code.sf.net/p/mingw-w64/mingw-w64
git_mirror model3emu svn https://svn.code.sf.net/p/model3emu/code/trunk
git_mirror moin-1.9 hg https://bitbucket.org/thomaswaldmann/moin-1.9
git_mirror moin-2.0 hg https://bitbucket.org/thomaswaldmann/moin-2.0
git_mirror ncurses git git://ncurses.scripts.mit.edu/ncurses.git
git_mirror newlib-cygwin git git://sourceware.org/git/newlib-cygwin.git
git_mirror nulldc svn http://nulldc.googlecode.com/svn/trunk
git_mirror odin svn https://svn.code.sf.net/p/odin-win/code/trunk
git_mirror patch git git://git.savannah.gnu.org/patch.git
git_mirror pcsx2 svn http://pcsx2.googlecode.com/svn/trunk
git_mirror pcsxr svn https://pcsxr.svn.codeplex.com/svn/pcsxr
git_mirror pinmame svn svn://svn.code.sf.net/p/pinmame/code/trunk
git_mirror plainamp svn svn://svn.code.sf.net/p/plainamp/code/trunk
git_mirror plibc svn https://svn.code.sf.net/p/plibc/code/trunk/plibc
git_mirror processhacker svn svn://svn.code.sf.net/p/processhacker/code
git_mirror qemu-android git https://android.googlesource.com/platform/external/qemu
git_mirror r svn https://svn.r-project.org/R/trunk
git_mirror rarfilesource git http://www.v12pwr.com/RARFileSource.git
git_mirror reactos svn svn://svn.reactos.org/reactos/trunk
git_mirror rtmpdump git git://git.ffmpeg.org/rtmpdump.git
git_mirror scintilla hg http://hg.code.sf.net/p/scintilla/code
git_mirror scite hg http://hg.code.sf.net/p/scintilla/scite
git_mirror sed git git://git.savannah.gnu.org/sed.git
git_mirror smartmontools svn https://svn.code.sf.net/p/smartmontools/code/trunk/smartmontools
git_mirror soundtouch svn https://svn.code.sf.net/p/soundtouch/code/trunk
git_mirror tclap git git://git.code.sf.net/p/tclap/code
git_mirror tinycc git git://repo.or.cz/tinycc.git
git_mirror vbam svn https://svn.code.sf.net/p/vbam/code
git_mirror vbox svn http://www.virtualbox.org/svn/vbox/trunk
git_mirror virtualjaguar git http://shamusworld.gotdns.org/git/virtualjaguar
git_mirror VMsvga2 svn svn://svn.code.sf.net/p/vmsvga2/code/VMsvga2/trunk
git_mirror wget git git://git.sv.gnu.org/wget.git
git_mirror wiimms-iso-tools svn http://opensvn.wiimm.de/wii/branches/public/wiimms-iso-tools
git_mirror winscp cvs :pserver:anonymous@winscp.cvs.sourceforge.net:/cvsroot/winscp winscp3
git_mirror x264 git http://git.videolan.org/git/x264.git
git_mirror xmlrpc-c svn http://svn.code.sf.net/p/xmlrpc-c/code '--ignore-paths=^(release_number|super_stable|userguide)'
git_mirror xmlstar git git://git.code.sf.net/p/xmlstar/code
git_mirror xserver git git://anongit.freedesktop.org/xorg/xserver

# The following projects are dead:
# git_mirror vba-rerecording svn http://vba-rerecording.googlecode.com/svn/trunk
