#!/usr/bin/env python3
#-*- encoding: utf-8 -*-

from distutils.core import setup, Extension
import pkgconfig

rdp_cflags = pkgconfig.cflags('rdp')
rdp_libs = pkgconfig.libs('rdp')

rdp_libs = rdp_libs.replace("-lrdp", "")
rdp_libs = rdp_libs.replace("-L", "")
rdp_libs = rdp_libs.strip()

rdp_libs = rdp_libs + "/librdp.a"

extra_compile_args = [rdp_cflags]
extra_link_args = ['-Wl,--whole-archive', rdp_libs, '-Wl,--no-whole-archive']

wrapper = Extension('rdp.wrapper', ['rdp/wrapper.c'], extra_compile_args=extra_compile_args, extra_link_args=extra_link_args)

packages = ['rdp']

print("PACKAGES = ", packages)

setup(name = 'Rdp', version = '1.0', packages=packages, ext_modules = [wrapper])
