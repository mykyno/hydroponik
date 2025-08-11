
  <Introduction>
    <Purpose>You are an expert software architect and codebase-structuring consultant. This document contains recommended principles and concrete suggestions for organizing a maintainable, testable, and scalable codebase.</Purpose>
   
  </Introduction>

  <Section id="1" name="DirectoryAndModularStructure">
    <Summary>Propose a clear, consistent folder layout that groups related functionality and aligns with common architectural patterns (feature-by-folder, layered, modular).</Summary>
    <Points>
      <Point>Feature-by-folder. when features are large and cross-cutting logic is minimal.</Point>
      <Point>Layered structure: when responsibilities are clear, separate by layer.</Point>
      <Point>Modular packages: for multi-package repos.</Point>
      <Point>Common/shared code: create <Code>src/common/</Code> or <Code>libs/</Code> for utilities, types, constants, and cross-cutting concerns (logging, errors).</Point>
      <Point>Configuration: centralize environment-specific settings in <Code>configs/</Code> and document expected env vars and formats.</Point>
      <Point>Tests location: mirror the source layout under <Code>tests/</Code> or colocate tests beside implementation (e.g., <Code>src/feature/feature.test.*</Code>), depending on team preference.</Point>
      <Point>Public entrypoints: keep a small set of top-level entry files (e.g., <Code>src/index</Code>, <Code>src/main</Code>, <Code>docker-entrypoint.sh</Code>).</Point>
    </Points>
  </Section>

  <Section id="2" name="SeparationOfConcernsAndSingleResponsibility">
    <Summary>Organize modules, classes, and functions so each has a single, well-defined purpose.</Summary>
    <Points>
      <Point>One responsibility per module/class: aim for small modules that do one job — easier to test and replace.</Point>
      <Point>Single Responsibility Principle: separate API/transport, business rules, persistence, and presentation layers.</Point>
      <Point>Use interfaces/ports: define explicit interfaces for external interactions (database, message buses) to decouple implementations.</Point>
      <Point>Composition over inheritance: prefer composing small responsibilities rather than deep inheritance hierarchies.</Point>
      <Point>Keep side-effects isolated: pure business logic functions should be side-effect free; side-effects live in adapter layers.</Point>
    </Points>
  </Section>

  <Section id="3" name="NamingConventionsAndCodingStyle">
    <Summary>Consistent naming and automated style enforcement reduce cognitive load and prevent style debates.</Summary>
    <Points>
      <Point>Naming patterns: use <Code>PascalCase</Code> for types/classes, <Code>camelCase</Code> for variables/functions, <Code>SCREAMING_SNAKE_CASE</Code> for constants and env vars (adjust per language norms).</Point>
      <Point>File naming: use kebab-case or snake_case for filenames unless language conventions differ (e.g., some ecosystems prefer PascalCase files for classes).</Point>
      <Point>Folder names: use meaningful, domain-oriented names (e.g., <Code>billing</Code>, <Code>user-profile</Code>).</Point>
      <Point>Linting & formatting: enforce a linter and formatter (ESLint + Prettier, pylint/black, rubocop, golangci-lint, etc.) with CI checks.</Point>
      <Point>Style guide: adopt a documented style guide in <Code>docs/STYLEGUIDE.md</Code> and include examples for common patterns.</Point>
      <Point>Automate: run linters and formatters locally (pre-commit) and in CI on every push or PR.</Point>
    </Points>
  </Section>

  <Section id="4" name="DRYAndReusability">
    <Summary>Avoid duplication by extracting stable abstractions and utilities; keep them discoverable and well-documented.</Summary>
    <Points>
      <Point>Extract common utilities to <Code>src/common/</Code> or shared modules/packages; avoid one-off helper copies across features.</Point>
      <Point>Prefer small, focused utility functions with clear names and tests.</Point>
      <Point>Abstract repeated workflows into higher-level services or libraries (e.g., retry logic, pagination, error wrappers).</Point>
      <Point>Don’t over-DRY: avoid premature abstraction; duplicate until you see a stable pattern, then extract.</Point>
      <Point>Version and package shared libraries when used across multiple services; maintain changelogs and semantic versioning.</Point>
    </Points>
  </Section>

  <Section id="5" name="DocumentationAndComments">
    <Summary>Document structure, rationale, and public APIs. Use comments to explain *why*, not *what*.</Summary>
    <Points>
      <Point>README files: include a top-level <Code>README.md</Code> (project overview, setup, run, test, deploy) and a README per major folder explaining purpose and conventions.</Point>
      <Point>API docs: use JSDoc, docstrings, Swagger/OpenAPI, or language-appropriate doc tools; publish generated docs to <Code>docs/</Code> or a static site.</Point>
      <Point>Inline comments: prefer short comments that explain intent, trade-offs, or non-obvious behavior; avoid noisy comments that restate code.</Point>
      <Point>Architecture docs: maintain a lightweight ADR (Architecture Decision Records) directory (<Code>docs/adr/</Code>) to capture major design decisions and reasons.</Point>
      <Point>Onboarding guide: provide quick-start and troubleshooting sections to help new contributors get productive fast.</Point>
    </Points>
  </Section>

  <Section id="6" name="TestingStrategy">
    <Summary>Place tests to mirror code organization; define a clear strategy for unit, integration, and system tests.</Summary>
    <Points>
      <Point>Test placement: either colocate tests next to code (e.g., <Code>src/module/module.test.*</Code>) or mirror under <Code>tests/</Code>; be consistent across the repo.</Point>
      <Point>Unit tests: fast, isolated, mocking external dependencies; run in local dev and CI on every commit.</Point>
      <Point>Integration tests: test interactions with real or test instances of databases, queues, and external APIs; run in CI pipelines at merge time or nightly.</Point>
      <Point>End-to-end tests: run in a staging environment mimicking production; keep them stable and limited in number.</Point>
      <Point>Test data: use fixtures and factories; avoid brittle hard-coded data; seed test databases in a reproducible way.</Point>
      <Point>Coverage & quality gates: track test coverage trends, but avoid enforcing arbitrary high thresholds that encourage meaningless tests.</Point>
    </Points>
  </Section>

  <Section id="7" name="VersionControlPractices">
    <Summary>Use predictable Git practices to keep history clean and encourage collaboration.</Summary>
    <Points>
      <Point>Commit often with small, focused changes; each commit should have a meaningful message describing the intent.</Point>
      <Point>Commit message style: follow a template or convention (e.g., Conventional Commits) to make changelogs and release automation easier.</Point>
      <Point>Branching strategy: choose one (GitFlow, GitHub Flow, trunk-based). Document it in <Code>docs/VERSION_CONTROL.md</Code>.</Point>
      <Point>Pull requests: require code review, passing CI checks (lint, tests), and descriptive PR titles and descriptions that explain why changes were made.</Point>
      <Point>Protect main branches: enforce required reviewers, status checks, and signed commits if needed.</Point>
    </Points>
  </Section>

  <Section id="8" name="RefactoringAndMaintenance">
    <Summary>Make refactoring a first-class activity and manage technical debt deliberately.</Summary>
    <Points>
      <Point>Allocate time: schedule small, regular refactor tasks rather than allowing debt to accumulate.</Point>
      <Point>Code ownership: keep modules small and assign ownership or maintainers for accountability.</Point>
      <Point>Technical debt register: track debt items in an issue tracker and prioritize them alongside features.</Point>
      <Point>Continuous cleanup: run dependency updates, remove dead code, and archive obsolete modules regularly.</Point>
      <Point>Refactor with tests: ensure comprehensive tests exist before large refactors to avoid regressions.</Point>
    </Points>
  </Section>

  <Section id="9" name="OptionalScalabilityAndArchitecturePatterns">
    <Summary>Discuss scalable patterns (layered, microservices, modular) and when to apply them.</Summary>
    <Points>
      <Point>Layered architecture: good for monoliths — presentation, application, domain, and infrastructure layers with clear boundaries.</Point>
      <Point>Modular monolith: structure by domain modules inside a single deployable process to balance modularity and operational simplicity.</Point>
      <Point>Microservices: adopt only when team size, deployment scale, and independent scaling or release cycles justify complexity; each service should have a clear bounded context.</Point>
      <Point>API contracts & versioning: design stable contracts (OpenAPI, Protobuf) and version APIs to support independent evolution.</Point>
      <Point>Observability: include logging, metrics, tracing, and health checks as first-class concerns for scalable systems.</Point>
      <Point>Data patterns: prefer domain-owned data stores, event-driven integration, and idempotent operations to reduce coupling.</Point>
    </Points>
  </Section>

  <Closing>
    <RecommendedNextSteps>
      <Step>Replace placeholders with the target language/framework and tweak naming/tool suggestions accordingly.</Step>
      <Step>Create a minimal example repo skeleton implementing these suggestions and add CI pipelines for linting, testing, and docs generation.</Step>
      <Step>Host documentation and ADRs where contributors can easily find and update them.</Step>
    </RecommendedNextSteps>
    <Contact>If you'd like, provide the target language or framework and I will generate a concrete repo skeleton XML -> folder/file manifest or a generator script.</Contact>
  </Closing>
</CodebaseGuidance>