# Fab Submission Checklist

This checklist is a release aid, not legal advice. Re-check the current official
Epic Games and Fab terms before every submission because requirements can change.

## Publisher and listing

- Confirm the publisher's legal name, tax, payout, and trader/non-trader details.
- Use a product title and artwork that do not imply Epic Games affiliation,
  certification, sponsorship, or endorsement.
- Provide valid public documentation and support contact URLs in the Fab listing.
- Describe features and limitations accurately; do not promise complete caller
  attribution where Unreal Engine runtime context can be unavailable.
- Declare generated or AI-assisted listing media when the current submission form
  requires it.
- Confirm commercial rights for the icon, screenshots, videos, fonts, sample
  projects, and every other uploaded asset.

## Plugin package

- Package only the plugin folder with the descriptor, `Source`, `Resources`, and
  release documentation needed by customers.
- Exclude `Binaries`, `Intermediate`, `Saved`, `DerivedDataCache`, IDE files,
  local logs, user configuration, and project-specific assets.
- Keep the descriptor module type as `Editor` and `CanContainContent` as `false`
  unless customer-facing content is intentionally added and audited.
- Build from a clean project for every Unreal Engine version claimed on the
  listing. Test editor startup, enable/disable, PIE start/end, and shutdown.
- Exercise all six Quick Diagnosis scenarios, cross-tool handoffs, session
  restore, Before/After snapshots, Stop All Active Work, and Support Bundle ZIP
  preview/export in the final UE 5.8 build.
- Open the generated support ZIP with the operating-system archive viewer and
  confirm that known local roots, email addresses, and secret assignments are
  absent before using it as a listing demonstration.
- Verify that no absolute workstation path, private project name, customer asset,
  credential, token, or personal data is present in source, docs, logs, or media.
- Update `PRIVACY.md` and the listing before adding telemetry, networking, cloud
  services, crash reporting, accounts, or payments.
- Update `THIRD_PARTY_NOTICES.md` before adding any external code or asset.

## Official pages to review manually

- Fab Distribution Agreement: https://www.fab.com/distribution-agreement
- Fab publisher documentation:
  https://dev.epicgames.com/documentation/en-us/fab/publisher-get-started-in-fab
- Fab asset structure requirements:
  https://dev.epicgames.com/documentation/en-us/fab/asset-file-format-and-structure-requirements-in-fab
- Unreal Engine branding guidelines: https://www.unrealengine.com/branding
- Epic Games content guidelines:
  https://legal.epicgames.com/en-US/epicgames/content-guidelines

## Blocking items before submission

- Replace or confirm the current `CreatedBy` value with the publisher name that
  should be visible to customers.
- Add real documentation and support URLs to the Fab listing. Do not submit empty,
  placeholder, private-network, or inaccessible URLs.
- Retain evidence showing the origin and commercial-use rights for
  `Resources/Icon128.png` and all listing media.
