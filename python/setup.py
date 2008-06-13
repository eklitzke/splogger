#!/usr/bin/python

from distutils.core import setup, Extension

__version__ = '0.1'

macros = [('MODULE_VERSION', '"%s"' % __version__)]

setup(
	name			= 'splogger',
	version			= __version__,
	author			= 'Evan Klitzke',
	author_email	= 'evan@eklitzke.org',
	description		= 'Python client to splogger',
	platforms		= ['Platform Independent'], # not quite true
	ext_modules		= [Extension(name='splogger', sources=['pysplog.c'], define_macros=macros)]
)
