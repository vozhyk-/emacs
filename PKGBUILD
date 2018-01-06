#######################################################################
# Track a maintenance branch or, by default, track master.
#
# Pick a branch from the output of "git branch" ran on your local copy
# of the emacs repository.
#
# E.g.:
#
# BRANCH=master
# BRANCH=emacs-26
#
# Take note that I don't expect you to track anything different to master
# or emacs-26 for obvious reasons. (See below).
#
BRANCH=master

#######################################################################
# Assign "YES" to the variable you want enabled; empty or other value
# for NO.
#
# Where you read experimental, replace with foobar.
# =================================================
#
#######################################################################
CHECK=            # Run tests.
CLANG=            # Use clang.
LTO=              # Enable link-time optimization. Experimental.
ATHENA=           # Use Athena widgets. (83 1337, 83 001d sk00l).
GTK2=             # Leave empty to compile with GTK+ 3 support.
                  # No, GTK+ 2 ain't kool, dawg!
GPM="YES"         # Enable gpm support.
M17N="YES"        # Enable m17n international table input support.
                  # You are far better off using UTF-8 and an input
                  # method under X/Wayland. But this gives independence
                  # if you need it.
OTF=              # OTF font support. Also a secondary dependency
                  # by pulling m17n-lib. Not needed in that case.
CAIRO="YES"       # Highly experimental. Maintaner dissapeared.
XWIDGETS="YES"    # Use GTK+ widgets pulled from webkit2gtk. Usable.
DOCS_HTML=        # Generate and install html documentation.
DOCS_PDF=         # Generate and install pdf documentation.
MAGICK="YES"      # Imagemagick, like flash, is bug ridden and won't die.
                  # Yet useful... Broken with the transition to IM7.
NOGZ=             # Don't compress el files.
#######################################################################

#######################################################################
if [[ BRANCH = "emacs-26" ]]; then
  pkgname=emacs26-git
else
  pkgname=emacs-pgtk
fi
pkgver=27.0.50.131782
pkgrel=1
pkgdesc="GNU Emacs. PGTK Development."
arch=('x86_64') # Arch Linux only. Users of derivatives are on their own.
url="https://github.com/masm11/emacs/"
license=('GPL3')
depends=( 'alsa-lib' )
makedepends=( 'git' )
#######################################################################

#######################################################################
if [[ $CLANG = "YES" ]]; then
  export CC=/usr/bin/clang ;
  export CXX=/usr/bin/clang++ ;
  export CPP="/usr/bin/clang -E" ;
  export LDFLAGS+=' -fuse-ld=lld' ;
  makedepends+=( 'clang' 'lld') ;
fi

if [[ $LTO = "yes" ]]; then
  export CFLAGS+=" -flto"
  export tXXFLAGS+=" -flto"
fi

if [[ $ATHENA = "YES" ]]; then
  depends+=( 'libxaw' );
elif [[ $GTK2 = "YES" ]]; then
  depends+=( 'gtk2' );
else
  depends+=( 'gtk3' );
fi

if [[ $GPM = "YES" ]]; then
  depends+=( 'gpm');
fi

if [[ $M17N = "YES" ]]; then
  depends+=( 'm17n-lib' );
fi

if [[ $OTF = "YES" ]] && [[ ! $M17N = "YES" ]]; then
  depends+=( 'libotf' );
fi

if [[ $MAGICK = "YES" ]]; then
  depends+=( 'imagemagick' );
  depends+=( 'libjpeg-turbo' 'giflib' );
elif [[ ! $NOX = "YES" ]]; then
  depends+=( 'libjpeg-turbo' 'giflib' );
else
  depends+=();
fi

if [[ $CAIRO = "YES" ]]; then
  depends+=( 'cairo' );
fi

if [[ $XWIDGETS = "YES" ]]; then
  if [[ $GTK2 = "YES" ]] || [[ $ATHENA = "YES" ]]; then
    echo "";
    echo "";
    echo "Xwidgets support *requires* gtk+3!!!";
    echo "";
    echo "";
    exit 1;
  else
    depends+=( 'webkit2gtk' );
  fi
fi

if [[ $DOCS_PDF = "YES" ]]; then
  makedepends+=( 'texlive-core' );
fi
#######################################################################

#######################################################################
conflicts=('emacs')
provides=('emacs')
source=("emacs-pgtk::git+https://github.com/masm11/emacs#branch=$BRANCH")
md5sums=('SKIP')

pkgver() {
  cd "$srcdir/emacs-pgtk"

  printf "%s.%s" \
    "$(grep AC_INIT configure.ac | \
    sed -e 's/^.\+\ \([0-9]\+\.[0-9]\+\.[0-9]\+\?\).\+$/\1/')" \
    "$(git rev-list --count HEAD)"
}


# There is no need to run autogen.sh after first checkout.
# Doing so, breaks incremental compilation.
prepare() {
  cd "$srcdir/emacs-pgtk"
  [[ -x configure ]] || ( ./autogen.sh git && ./autogen.sh autoconf )
}

build() {
  cd "$srcdir/emacs-pgtk"

  local _conf=(
    --prefix=/opt/emacs-pgtk
    --sysconfdir=/etc
    --libexecdir=/opt/emacs-pgtk/lib
    --localstatedir=/var
    --mandir=/opt/emacs-pgtk/share/man
    --without-x
    --with-gameuser=:games
    --with-sound=alsa
    --with-xft
    --with-modules
  )

#######################################################################

#######################################################################
if [[ $CLANG = "YES" ]]; then
  _conf+=(
    '--enable-autodepend'
 );
fi
if [[ $LTO = "YES" ]]; then
  _conf+=(
    '--enable-link-time-optimization'
  );
fi

# Beware https://debbugs.gnu.org/cgi/bugreport.cgi?bug=25228
# dconf and gconf break font settings you set in ~/.emacs.
# If you insist you'll need to play gymnastics with
# set-frame-font and set-menu-font. Good luck!
if [[ $ATHENA = "YES" ]]; then
  _conf+=( '--with-x-toolkit=athena' '--without-gconf' '--without-gsettings' );
elif [[ $GTK2 = "YES" ]]; then
  _conf+=( '--with-x-toolkit=gtk2' '--without-gconf' '--without-gsettings' );
else
  # _conf+=( '--with-x-toolkit=gtk3' '--without-gconf' '--without-gsettings' );
  _conf+=( '--with-x-toolkit=gtk3' );
fi

if [[ ! $GPM = "YES" ]]; then
  _conf+=( '--without-gpm' );
fi

if [[ ! $M17N = "YES" ]]; then
  _conf+=( '--without-m17n-flt' );
fi

if [[ $MAGICK = "YES" ]]; then
  _conf+=(
    '--with-imagemagick'
 );
else
  _conf+=( '--without-imagemagick' );
fi

if [[ $CAIRO = "YES" ]]; then
  _conf+=( '--with-cairo' );
fi

if [[ $XWIDGETS = "YES" ]]; then
  _conf+=( '--with-xwidgets' );
fi

if [[ $NOGZ = "YES" ]]; then
  _conf+=( '--without-compress-install' );
fi
#######################################################################

#######################################################################

  # Use gold with gcc, unconditionally.
  #
  if [[ ! $CLANG = "YES" ]]; then
    export LD=/usr/bin/ld.gold
    export LDFLAGS+=" -fuse-ld=gold";
  fi

  ./configure "${_conf[@]}"

  # Using "make" instead of "make bootstrap" enables incremental
  # compiling. Less time recompiling. Yay! But you may
  # need to use bootstrap sometimes to unbreak the build.
  # Just add it to the command line.
  #
  # Please note that incremental compilation implies that you
  # are reusing your src directory!
  #
  make

  # You may need to run this if 'loaddefs.el' files corrupt.
  #cd "$srcdir/emacs-pgtk/lisp"
  #make autoloads
  #cd ../

  # Optional documentation formats.
  if [[ $DOCS_HTML = "YES" ]]; then
    make html;
  fi
  if [[ $DOCS_PDF = "YES" ]]; then
    make pdf;
  fi
  if [[ $CHECK = "YES" ]]; then
    make check;
  fi

}

package() {
  cd "$srcdir/emacs-pgtk"

  make DESTDIR="$pkgdir/" install

  # Install optional documentation formats
  if [[ $DOCS_HTML = "YES" ]]; then make DESTDIR="$pkgdir/" install-html; fi
  if [[ $DOCS_PDF = "YES" ]]; then make DESTDIR="$pkgdir/" install-pdf; fi

  # remove conflict with ctags package
  mv "$pkgdir"/opt/emacs-pgtk/bin/{ctags,ctags.emacs}

  if [[ $NOGZ = "YES" ]]; then
    mv "$pkgdir"/opt/emacs-pgtk/share/man/man1/{ctags.1,ctags.emacs.1};
  else
    mv "$pkgdir"/opt/emacs-pgtk/share/man/man1/{ctags.1.gz,ctags.emacs.1.gz}
  fi

  # fix user/root permissions on usr/share files
  find "$pkgdir"/opt/emacs-pgtk/share/emacs/ | xargs chown root:root

  # fix permssions on /var/games
  mkdir -p "$pkgdir"/var/games/emacs
  chmod 775 "$pkgdir"/var/games
  chmod 775 "$pkgdir"/var/games/emacs
  chown -R root:games "$pkgdir"/var/games

  # The logic used to install systemd's user service is partially broken
  # under Arch Linux model, because it adds $DESTDIR as prefix to the
  # final Exec targets. The fix is to hack it with an axe.
  install -Dm644 etc/emacs.service \
    "$pkgdir"/opt/emacs-pgtk/lib/systemd/user/emacs.service
  sed -i -e 's#\(ExecStart\=\)#\1\/opt/emacs-pgtk\/bin\/#' \
    -e 's#\(ExecStop\=\)#\1\/opt/emacs-pgtk\/bin\/#' \
    "$pkgdir"/opt/emacs-pgtk/lib/systemd/user/emacs.service
}

# vim:set ft=sh ts=2 sw=2 et:
