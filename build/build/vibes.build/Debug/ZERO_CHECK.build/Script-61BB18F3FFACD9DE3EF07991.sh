#!/bin/sh
set -e
if test "$CONFIGURATION" = "Debug"; then :
  cd /Users/justus/dev/vibes/build
  make -f /Users/justus/dev/vibes/build/CMakeScripts/ReRunCMake.make
fi
if test "$CONFIGURATION" = "Release"; then :
  cd /Users/justus/dev/vibes/build
  make -f /Users/justus/dev/vibes/build/CMakeScripts/ReRunCMake.make
fi
if test "$CONFIGURATION" = "MinSizeRel"; then :
  cd /Users/justus/dev/vibes/build
  make -f /Users/justus/dev/vibes/build/CMakeScripts/ReRunCMake.make
fi
if test "$CONFIGURATION" = "RelWithDebInfo"; then :
  cd /Users/justus/dev/vibes/build
  make -f /Users/justus/dev/vibes/build/CMakeScripts/ReRunCMake.make
fi

