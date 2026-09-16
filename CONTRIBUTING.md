# Contributing to Oryx

Thank you for your interest in contributing to Oryx! This document outlines guidelines and processes for contributing to the project.

## Code of Conduct

We are committed to providing a welcoming and inclusive environment for all contributors. Please be respectful and constructive in all interactions.

## How to Contribute

### Reporting Bugs

If you find a bug:

1. Check the [issue tracker](https://github.com/JackBehindWood/oryx/issues) to see if the bug has already been reported.
2. If not, open a new issue with:
   - Clear title describing the bug
   - Detailed description of the problem
   - Steps to reproduce
   - Expected vs. actual behavior
   - Environment (OS, compiler version, Python version, etc.)
   - Minimal reproducible example if possible

### Suggesting Features

To suggest a feature or improvement:

1. Check existing [issues](https://github.com/JackBehindWood/oryx/issues) and [discussions](https://github.com/JackBehindWood/oryx/discussions).
2. Open a new discussion or issue with:
   - Clear description of the feature
   - Motivation and use case
   - Example of how it would be used
   - Any relevant context or references

### Submitting Changes

#### Setup

1. Fork the repository on GitHub.
2. Clone your fork locally:
   ```bash
   git clone https://github.com/your-username/oryx.git
   cd oryx
   git submodule update --init --recursive
   ```
   (`tests/vendor/doctest`, the test framework, is a git submodule — without
   this step `uv run build build compile` will fail with a clear message
   telling you to run it.)