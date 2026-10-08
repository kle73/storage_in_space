# Contributing to the Quantum Network Simulation Module for ns-3

Welcome! We're excited that you're interested in contributing to this Quantum Network Simulation Module built for ns-3. This guide outlines how to report issues, suggest features, and contribute code.

---

## 🐞 Reporting Bugs / Requesting Features

Use the [**GitLab Issues**](https://gitlab.com/mohittahiliani/quantum/-/issues) tab to report bugs or request features.

Please include:
- A descriptive title
- Environment details (OS, ns-3 version, commit hash)
- Steps to reproduce (for bugs)
- Suggested behavior (for features)

---

## 🧪 Development Setup

### Building the Simulator

This module is designed to work with ns-3 (ns-3.45 or later).

```bash
./ns3 configure --enable-examples --enable-qpp --enable-crypto
./ns3 build
```
Run the quantum examples
```bash
./ns3 run qkd-example
./ns3 run qkd-eavesdropper-example
```

Ensure you're using the right commit of ns-3-dev or the appstore integration instructions provided in our README.

## 🚀 Submitting Contributions

### Workflow

1. Fork the repo.
2. Make your changes (code, tests, examples, docs).
3. Ensure your code builds and passes all tests.
4. Commit with a descriptive message.
5. Open a Merge Request on GitLab, linking any related issue.

### Commit Message Style

Use clear and conventional commit messages, e.g.:

```
qkd: add support for BB84 protocol
docs: update README with simulation instructions
```

## ✅ PR Checklist

Before submitting:
- [ ] Code compiles and runs
- [ ] New features have examples or tests
- [ ] Docs updated (README.md, inline Doxygen)
- [ ] Follows ns-3 coding style
- [ ] LICENSE headers included in new files

## 🧱 Code Style

We follow the [ns-3 coding conventions](https://www.nsnam.org/docs/contributing/html/coding-style.html#coding-style):
- 2-space indentation
- K&R brace style
- Doxygen-style comments
- Use NS_LOG, NS_ASSERT, etc. as per ns-3 guidelines

Run clang-format if applicable to maintain consistency.

## 📝 Documentation

- Code should be well-commented with Doxygen
- Update README.md and example descriptions as needed
- Use doc/ for extended usage or design documents
