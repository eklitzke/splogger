#!/usr/bin/python

from distutils.core import setup, Extension
import os

__version__ = '0.1'

macros = [('MODULE_VERSION', '"%s"' % __version__)]

splogger_extension = Extension(
	name='splogger',
	sources=['pysplog.c'],
	libraries=['spread'],
	library_dirs=[os.path.join(os.environ['HOME'], 'local/lib')], #FIXME: maybe there's a better way to do this?
	define_macros=macros
)

setup(
	name			= 'splogger',
	version			= __version__,
	author			= 'Evan Klitzke',
	author_email	= 'evan@eklitzke.org',
	description		= 'Python client to splogger',
	platforms		= ['Platform Independent'], # FIXME: not quite true
	ext_modules		= [splogger_extension],
    library_dirs=['/usr/local/lib'],
)
