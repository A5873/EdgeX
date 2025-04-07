# Contributing to EdgeX

Thank you for your interest in contributing to EdgeX! This document provides guidelines and instructions for contributing to the project.

## Maintainers

- [@A5873](https://github.com/A5873) - Alex Ngugi

## Code of Conduct

We expect all contributors to adhere to the following code of conduct:

- Be respectful and inclusive
- Provide constructive feedback
- Focus on the technical merits of contributions
- Communicate clearly and effectively

## Getting Started

1. Fork the repository on GitHub
2. Clone your fork to your local machine
3. Set up the development environment as described in the README.md

## Development Workflow

1. Create a new branch for your feature or bugfix
2. Make your changes, following the coding standards
3. Write or update tests for your changes
4. Ensure all tests pass with `make test`
5. Commit your changes with clear, descriptive messages
6. Push your branch to your fork
7. Create a pull request against the main repository

## Coding Standards

- **C Code**:
  - Follow K&R style with 4-space indentation
  - Keep functions small and focused
  - Document all public functions with clear comments
  - Avoid global variables where possible
  
- **Rust Code**:
  - Follow Rust standard style (rustfmt)
  - Use meaningful variable and function names
  - Document all public interfaces

## Pull Request Process

1. Ensure your code compiles without warnings
2. Make sure all tests pass
3. Update documentation where necessary
4. Your PR should contain a single logical change
5. Include benchmark results for performance-critical changes
6. Address review comments promptly

## Testing Requirements

- All new features must include tests
- Bugfixes should include a test that would have caught the bug
- Tests should be fast and deterministic
- Avoid tests that depend on timing or specific hardware

## Documentation

- Update the README.md for user-facing changes
- Document APIs in header files
- Add architecture notes to ARCHITECTURE.md for significant changes

## Review Process

Pull requests require review from at least one maintainer. Reviews will focus on:

1. Correctness
2. Performance
3. Security implications
4. Maintainability
5. Test coverage

## License

By contributing to EdgeX, you agree that your contributions will be licensed under the MIT License found in the LICENSE file.

