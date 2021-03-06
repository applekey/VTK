# Copyright (c) Twisted Matrix Laboratories.
# See LICENSE for details.

"""
Tests for L{twisted.python._inotify}.
"""

from twisted.trial.unittest import TestCase
from twisted.python.runtime import platform

if platform.supportsINotify():
    from ctypes import c_int, c_uint32, c_char_p
    from twisted.python import _inotify
    from twisted.python._inotify import (
        INotifyError, initializeModule, init, add)
else:
    _inotify = None



class INotifyTests(TestCase):
    """
    Tests for L{twisted.python._inotify}.
    """
    if _inotify is None:
        skip = "This platform doesn't support INotify."

    def test_missingInit(self):
        """
        If the I{libc} object passed to L{initializeModule} has no
        C{inotify_init} attribute, L{ImportError} is raised.
        """
        class libc:
            def inotify_add_watch(self):
                pass
            def inotify_rm_watch(self):
                pass
        self.assertRaises(ImportError, initializeModule, libc())


    def test_missingAdd(self):
        """
        If the I{libc} object passed to L{initializeModule} has no
        C{inotify_add_watch} attribute, L{ImportError} is raised.
        """
        class libc:
            def inotify_init(self):
                pass
            def inotify_rm_watch(self):
                pass
        self.assertRaises(ImportError, initializeModule, libc())


    def test_missingRemove(self):
        """
        If the I{libc} object passed to L{initializeModule} has no
        C{inotify_rm_watch} attribute, L{ImportError} is raised.
        """
        class libc:
            def inotify_init(self):
                pass
            def inotify_add_watch(self):
                pass
        self.assertRaises(ImportError, initializeModule, libc())


    def test_setTypes(self):
        """
        If the I{libc} object passed to L{initializeModule} has all of the
        necessary attributes, it sets the C{argtypes} and C{restype} attributes
        of the three ctypes methods used from libc.
        """
        class libc:
            def inotify_init(self):
                pass
            inotify_init = staticmethod(inotify_init)

            def inotify_rm_watch(self):
                pass
            inotify_rm_watch = staticmethod(inotify_rm_watch)

            def inotify_add_watch(self):
                pass
            inotify_add_watch = staticmethod(inotify_add_watch)

        c = libc()
        initializeModule(c)
        self.assertEqual(c.inotify_init.argtypes, [])
        self.assertEqual(c.inotify_init.restype, c_int)

        self.assertEqual(c.inotify_rm_watch.argtypes, [c_int, c_int])
        self.assertEqual(c.inotify_rm_watch.restype, c_int)

        self.assertEqual(
            c.inotify_add_watch.argtypes, [c_int, c_char_p, c_uint32])
        self.assertEqual(c.inotify_add_watch.restype, c_int)


    def test_failedInit(self):
        """
        If C{inotify_init} returns a negative number, L{init} raises
        L{INotifyError}.
        """
        class libc:
            def inotify_init(self):
                return -1
        self.patch(_inotify, 'libc', libc())
        self.assertRaises(INotifyError, init)


    def test_failedAddWatch(self):
        """
        If C{inotify_add_watch} returns a negative number, L{add}
        raises L{INotifyError}.
        """
        class libc:
            def inotify_add_watch(self, fd, path, mask):
                return -1
        self.patch(_inotify, 'libc', libc())
        self.assertRaises(INotifyError, add, 3, '/foo', 0)
