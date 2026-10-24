"""Deterministic LSL publishers for testing the C++ viewer.

This package deliberately contains no viewer logic: it only builds stream
descriptions and pushes samples. Any code here that tried to interpret
metadata would turn the tests into a copy of the thing under test.
"""
