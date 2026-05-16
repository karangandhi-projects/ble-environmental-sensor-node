# Human Reading Path

Read in this order:

1. `README.md`
2. `AGENT_BRIEF.md`
3. `docs/vision.md`
4. `docs/requirements.md`
5. `docs/design_decisions.md`
6. `docs/architecture.md`
7. `docs/gatt_profile.md`
8. `docs/implementation_plan.md`
9. `docs/build_and_flash.md`
10. `docs/test_plan.md`

After that, inspect firmware code.

The most important design idea is separation of concerns:
- BLE is not the application.
- Sensors are not BLE.
- Storage is not BLE.
- App state connects all components through clear interfaces.
