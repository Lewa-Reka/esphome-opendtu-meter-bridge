# Changelog

All notable changes to this fork are documented here.

## v0.2.0 - 2026-08-04

### Added

- Selectable `sdm630` and `dtsu666` meter profiles through `meter_profile`.
- CHINT DTSU666 instantaneous voltage, current, active-power, and frequency register mapping based on the published CHINT Modbus specification.
- Separate profile descriptor tables, a shared `meter_profile_codec`, and CI build configurations for SDM630 and DTSU666.
- Golden-vector protocol tests for exact register words, FP32 encoding, profile windows, and boundary handling.
- A compatibility wrapper for existing `components: [opendtu_sdm630]` and `opendtu_sdm630:` configurations.
- Migration, Modbus behavior, release, and profile-extension documentation.

### Changed

- Introduced `opendtu_meter_bridge` / OpenDTU Meter Bridge as the canonical ESPHome component, C++ namespace, class, file, log-tag, reference-configuration, and device-facing naming.
- Raised the documented and tested minimum ESPHome version to 2025.6.3.
- Added compatibility with the ESPHome Modbus server API introduced in newer releases while retaining the older supported API path.
- Made DTSU666 current registers positive RMS magnitudes while retaining directional active power and the legacy SDM630 current convention.
- Hardened WebSocket callback handoff with synchronized state, bounded/coalesced event queuing, and validated fragmented frames.
- Removed fixed-size Basic Auth buffers, marked the password as sensitive in supported ESPHome versions, and validated Modbus slave addresses and read quantities.

### Deprecated

- The legacy `opendtu_sdm630` component/domain remains functional through a compatibility wrapper, emits a deprecation warning, and defaults to the SDM630 profile.
- `meter_type` remains accepted as a deprecated alias for `meter_profile`, with the same `sdm630` and `dtsu666` values.
- Both deprecated aliases remain supported throughout all 0.2.x releases and may be removed no earlier than v0.3.0.

### Compatibility

- Existing v0.0.1 configurations can use the 0.2.x fork without changing `components: [opendtu_sdm630]` or the `opendtu_sdm630:` domain. They receive a deprecation warning and retain SDM630 as the default profile.
- New configurations should use `components: [opendtu_meter_bridge]`, the `opendtu_meter_bridge:` domain, and `meter_profile`.
- The legacy and neutral component domains cannot be configured together in one ESPHome node.

### Validation status

- The SDM630 profile has been validated with the documented Deye setup.
- The DTSU666 profile is implemented from the published specification and has not yet been validated against physical DTSU666 hardware.
- Both neutral profiles and the legacy-domain compatibility path are compiled in CI against ESPHome stable, the pinned 2026.7.3 release, and the minimum supported ESPHome version.
- Host-side golden-vector tests validate the profile codec and exact SDM630/DTSU666 register output.

## v0.0.1 - 2026-06-06

- Original upstream release by Lewa-Reka.
- Added the `opendtu_sdm630` ESPHome external component for exposing OpenDTU measurements through an Eastron SDM630-compatible Modbus register map.
