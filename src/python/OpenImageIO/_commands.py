import os
import sys
import subprocess


def _program(name, args):
    return subprocess.call([os.path.join(os.path.dirname(__file__), 'commands', name)] + args)


def oiiotool():
    raise SystemExit(_program('oiiotool', sys.argv[1:]))


def iinfo():
    raise SystemExit(_program('iinfo', sys.argv[1:]))


def testtex():
    raise SystemExit(_program('testtex', sys.argv[1:]))


def maketx():
    raise SystemExit(_program('maketx', sys.argv[1:]))


def idiff():
    raise SystemExit(_program('idiff', sys.argv[1:]))


def igrep():
    raise SystemExit(_program('igrep', sys.argv[1:]))


def iconvert():
    raise SystemExit(_program('iconvert', sys.argv[1:]))
