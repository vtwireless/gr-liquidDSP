
# The presence of this file turns this directory into a Python package

'''
This is the GNU Radio PIPE module. Place your Python package
description here (python/__init__.py).
'''
from __future__ import unicode_literals

# import swig generated symbols into the pipe namespace
try:
    # this might fail if the module is python-only
    from .pipe_swig import *
except ImportError:
    pass

# import any pure python here
#
