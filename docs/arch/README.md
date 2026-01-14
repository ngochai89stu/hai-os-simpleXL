# Architecture Diagrams (PlantUML)

This folder contains PlantUML diagrams describing the SimpleXL firmware architecture.

## Diagrams

1. `c4_container.puml`
   - High-level component layout (sx_core / sx_services / sx_ui / sx_platform)

2. `event_flow.puml`
   - Sequence diagram of the event-driven architecture

## Generating

Locally:
```bash
plantuml -tpng docs/arch/*.puml
```

CI:
- GitHub Actions generates PNG artifacts automatically.
