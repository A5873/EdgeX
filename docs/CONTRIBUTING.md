# Contributing to EdgeX

Thank you for your interest in contributing to EdgeX! This document provides comprehensive guidelines and instructions for contributing to our secure, microkernel-based operating system for edge AI devices.

## Table of Contents

- [Maintainers](#maintainers)
- [Code of Conduct](#code-of-conduct)
- [Communication Channels](#communication-channels)
- [Getting Started](#getting-started)
- [Development Workflow](#development-workflow)
- [Architecture-Specific Guidelines](#architecture-specific-guidelines)
- [Security Contribution Guidelines](#security-contribution-guidelines)
- [Coding Standards](#coding-standards)
- [Pull Request Process](#pull-request-process)
- [Bug Reporting](#bug-reporting)
- [Feature Requests](#feature-requests)
- [Performance Benchmarking](#performance-benchmarking)
- [Testing Requirements](#testing-requirements)
- [Documentation Standards](#documentation-standards)
- [Code Review Checklist](#code-review-checklist)
- [License](#license)

## Maintainers

- [@A5873](https://github.com/A5873) - Alex Ngugi

## Code of Conduct

We expect all contributors to adhere to the following code of conduct:

- Be respectful and inclusive
- Provide constructive feedback
- Focus on the technical merits of contributions
- Communicate clearly and effectively
- Support fellow contributors and foster a collaborative environment

## Communication Channels

- **GitHub Issues**: For bug reports, feature requests, and task tracking
- **GitHub Discussions**: For design discussions and community Q&A
- **Discord**: [Join our Discord server](https://discord.gg/edgex) for real-time communication
- **Mailing List**: Subscribe to our [developer mailing list](mailto:dev@edgex.io) for announcements
- **Bi-weekly Meetings**: Every other Tuesday at 15:00 UTC on [Zoom](https://zoom.us/j/123456789)

## Getting Started

1. Fork the repository on GitHub
2. Clone your fork to your local machine: `git clone https://github.com/YOUR-USERNAME/edgex.git`
3. Set up the development environment as described in the README.md
4. Add the upstream repository: `git remote add upstream https://github.com/edgex/edgex.git`
5. Install the pre-commit hooks: `make install-hooks`
6. Build the project to verify your setup: `make all`

## Development Workflow

1. Sync with the latest upstream changes: `git fetch upstream && git rebase upstream/main`
2. Create a new branch for your feature or bugfix: `git checkout -b feature/your-feature-name`
3. Make your changes, following the coding standards
4. Write or update tests for your changes
5. Ensure all tests pass with `make test`
6. Verify your changes work on appropriate hardware or emulators
7. Run static analysis tools: `make lint`
8. Commit your changes with clear, descriptive messages following [conventional commits](https://www.conventionalcommits.org/)
9. Push your branch to your fork: `git push origin feature/your-feature-name`
10. Create a pull request against the main repository

### Commit Message Format

We follow the Conventional Commits specification:

```
<type>(<scope>): <description>

[optional body]

[optional footer(s)]
```

Examples:
- `feat(scheduler): implement quantum-resistant task scheduling`
- `fix(arm64): correct memory alignment issue in crypto module`
- `docs(contributing): update architecture-specific guidelines`
- `perf(x86_64): optimize cache utilization in IPC pathway`

## Architecture-Specific Guidelines

EdgeX supports multiple architectures, each with specific considerations:

### x86_64

- Test on actual hardware when possible, not just emulators
- Consider both legacy BIOS and UEFI boot methods
- Pay attention to CPU feature detection and fallbacks
- Optimize for modern x86_64 features (AVX, etc.) with fallbacks

### ARM64

- Test on representative devices (Raspberry Pi 4, Jetson Nano, etc.)
- Consider big.LITTLE architectures for scheduling optimizations
- Pay attention to ARM-specific memory ordering rules
- Leverage NEON instructions for performance when appropriate

### RISC-V

- Follow RISC-V specification strictly for maximum compatibility
- Test on multiple implementation variants when possible
- Avoid architecture-specific assumptions from x86/ARM experience
- Optimize for reduced instruction set while maintaining performance

### Cross-Architecture Considerations

- Avoid architecture-specific code in shared components
- Use appropriate abstraction layers for hardware-specific operations
- Test on all supported architectures before submitting PRs
- Document architecture-specific behaviors and requirements

## Security Contribution Guidelines

EdgeX prioritizes security with a focus on quantum-resistant cryptography:

### Cryptographic Implementations

- All cryptographic code must be reviewed by at least two maintainers
- Prefer quantum-resistant algorithms from the NIST PQC standardization process
- Document cryptographic design decisions and implementation details
- Include test vectors for cryptographic operations
- Avoid implementing your own cryptographic primitives

### Security Review Requirements

- Security-critical changes require dedicated security review
- Document threat models for security-sensitive components
- Include mitigations for side-channel attacks where appropriate
- Consider both software and hardware attack vectors

### Secure Development Practices

- Follow the principle of least privilege
- Validate all inputs, especially those crossing trust boundaries
- Avoid unsafe memory operations and potential buffer overflows
- Initialize all variables and memory allocations properly
- Free all allocated resources properly to avoid leaks

## Coding Standards

### C Code

- Follow K&R style with 4-space indentation
- Keep functions small and focused (<100 lines when possible)
- Document all public functions with clear comments
- Avoid global variables where possible
- Use static analysis tools (clang-analyzer, cppcheck)
- Follow secure coding practices (CERT C, MISRA C)

### Rust Code

- Follow Rust standard style (use `rustfmt`)
- Use meaningful variable and function names
- Document all public interfaces
- Minimize use of `unsafe` blocks, and document when necessary
- Leverage Rust's ownership model for memory safety
- Use the type system to prevent errors at compile time

### Assembly Code

- Comment extensively to explain non-obvious operations
- Use standard assembly directives for each architecture
- Label branches and sections clearly
- Document register usage and calling conventions
- Provide C fallbacks for critical assembly sections

## Pull Request Process

1. Ensure your code compiles without warnings on all supported architectures
2. Make sure all tests pass: unit tests, integration tests, and architecture-specific tests
3. Update documentation where necessary
4. Your PR should contain a single logical change
5. Include benchmark results for performance-critical changes
6. Address review comments promptly
7. Ensure CI passes on all supported platforms
8. Use the PR template provided

### PR Template

When creating a PR, use this template:

```markdown
## Description

[Describe your changes in detail]

## Related Issue

Fixes #[issue number]

## Type of Change

- [ ] Bug fix
- [ ] New feature
- [ ] Performance improvement
- [ ] Documentation update
- [ ] Code refactoring
- [ ] Other (please describe)

## Architectures Affected

- [ ] x86_64
- [ ] ARM64
- [ ] RISC-V
- [ ] All
- [ ] None (documentation, build system, etc.)

## Testing Performed

- [ ] Unit tests
- [ ] Integration tests
- [ ] Performance benchmarks
- [ ] Manual testing (please describe)
- [ ] Tested on actual hardware (please specify)

## Benchmark Results

[If applicable, include before/after benchmark results]

## Documentation Updates

[Describe any documentation changes or indicate "N/A"]

## Security Considerations

[Describe any security implications or indicate "N/A"]
```

## Bug Reporting

When reporting bugs, please use the bug report template and include:

1. EdgeX version and environment details
2. Steps to reproduce the issue
3. Expected vs. actual behavior
4. Relevant logs, screenshots, or other diagnostic information
5. Hardware architecture affected
6. Suggested fix (if you have one)

### Bug Report Template

```markdown
## Bug Description

[Clear and concise description of the bug]

## Environment

- EdgeX Version: [e.g., v0.1.0]
- Architecture: [e.g., x86_64, ARM64, RISC-V]
- Hardware: [e.g., Intel NUC, Raspberry Pi 4, QEMU]
- OS: [if running host OS, e.g., Ubuntu 24.04]

## Steps to Reproduce

1. [First step]
2. [Second step]
3. [and so on...]

## Expected Behavior

[What you expected to happen]

## Actual Behavior

[What actually happened]

## Logs/Output

```
[Paste relevant logs, output, or error messages here]
```

## Additional Context

[Any other context about the problem]
```

## Feature Requests

Feature requests should align with the EdgeX roadmap and architecture. When requesting features:

1. Check existing issues to avoid duplicates
2. Clearly describe the problem the feature would solve
3. Explain how your feature fits EdgeX's microkernel architecture
4. Indicate which architectures would benefit
5. Consider security, performance, and maintenance implications

### Feature Request Template

```markdown
## Feature Description

[Clear and concise description of the requested feature]

## Problem Statement

[Describe the problem this feature would solve]

## Proposed Solution

[Describe your proposed solution]

## Alternatives Considered

[Describe alternative approaches you've considered]

## Affected Architectures

- [ ] x86_64
- [ ] ARM64
- [ ] RISC-V
- [ ] All

## Security Implications

[Describe any security implications]

## Performance Considerations

[Describe any performance considerations]

## Additional Context

[Any other relevant information]
```

## Performance Benchmarking

Performance is critical for EdgeX. For performance-related changes:

1. Establish baseline performance with existing code
2. Measure performance impact of your changes
3. Document methodology, environment, and tools used
4. Compare results across supported architectures when applicable
5. Consider both throughput and latency/jitter metrics
6. Include both best-case and worst-case scenarios

### Required Benchmarks

For core components, include at least:

- Memory usage impact (increase/decrease)
- CPU utilization impact
- Execution time difference
- System call frequency changes
- Impact on real-time guarantees (if applicable)

### Benchmark Tools

- Use `edgex-bench` for standardized benchmarking
- Include hardware specifications in benchmark reports
- Document compiler optimizations used
- Report statistical significance (multiple runs)

## Testing Requirements

- All new features must include tests
- Bugfixes should include a test that would have caught the bug
- Tests should be fast and deterministic
- Avoid tests that depend on timing or specific hardware
- Core components require both unit and integration tests
- Security-critical components require penetration tests
- Architecture-specific features require tests on that architecture

### Test Categories

1. **Unit Tests**: For individual functions and components
2. **Integration Tests**: For subsystem interactions
3. **System Tests**: For complete system behavior
4. **Performance Tests**: For measuring performance metrics
5. **Security Tests**: For validating security properties
6. **Architecture-specific Tests**: For architecture-dependent features

## Documentation Standards

- Update the README.md for user-facing changes
- Document APIs in header files
- Add architecture notes to ARCHITECTURE.md for significant changes
- Use Markdown for all documentation
- Include diagrams for complex components (using PlantUML or Mermaid)
- Provide examples for new APIs or features
- Include performance expectations and benchmarks where applicable
- Document security considerations for sensitive components

### Documentation Structure

- API documentation should follow the format:
  - Purpose
  - Parameters
  - Return values
  - Error conditions
  - Side effects
  - Thread safety
  - Example usage

- Architecture documentation should include:
  - Component relationships
  - Data flow diagrams
  - Threat models (for security-sensitive components)
  - Performance characteristics

## Code Review Checklist

Pull requests require review from at least one maintainer. Reviews will focus on:

1. **Correctness**
   - Code functions as intended
   - Edge cases are handled
   - Error conditions are managed properly

2. **Performance**
   - No unnecessary operations
   - Efficient algorithms and data structures
   - No unexpected regression in benchmarks
   - Architecture-specific optimizations where appropriate

3. **Security**
   - No introduction of security vulnerabilities
   - Proper input validation
   - Secure cryptographic implementations
   - Protection against common attack vectors

4. **Maintainability**
   - Code readability and organization
   - Meaningful variable/function names
   - Appropriate comments and documentation
   - No unnecessary complexity

5. **Test Coverage**
   - Tests cover normal operation
   - Tests cover error conditions
   - Tests cover edge cases
   - Tests are deterministic and reliable

## License

By contributing to EdgeX, you agree that your contributions will be licensed under the MIT License found in the LICENSE file.

---

Thank you for contributing to EdgeX! Your efforts help us build a secure, efficient operating system for the future of edge computing and AI.
