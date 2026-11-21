# Integrations

## Jira / Xray

| | Jira Cloud + Xray Cloud | Jira Data Center + Xray Server/DC |
|---|---|---|
| `jira.deployment` | `cloud` | `datacenter` |
| Test search | `GET {baseUrl}/rest/api/3/search/jql` (token-based paging) | `GET {baseUrl}/rest/api/2/search` |
| Jira auth | `RG_JIRA_USER` (e-mail) + `RG_JIRA_TOKEN` (API token) → Basic | `RG_JIRA_TOKEN` (personal access token) → Bearer |
| Result import | `POST {xrayBaseUrl}/api/v2/import/execution` after `POST /api/v2/authenticate` | `POST {baseUrl}/rest/raven/1.0/import/execution` |
| Xray auth | `RG_XRAY_CLIENT_ID` / `RG_XRAY_CLIENT_SECRET` (Xray API key) | same as Jira |
| Default statuses | PASS→`PASSED`, FAIL→`FAILED`, ERROR→`FAILED`, BLOCKED→`TODO` | PASS→`PASS`, FAIL→`FAIL`, ERROR→`ABORTED`, BLOCKED→`TODO` |
| Evidence field | `evidence` | `evidences` |

Atlassian removed the old Cloud endpoint `/rest/api/3/search`, so Robo Guide uses `/search/jql`
on Cloud.

If your Xray instance has custom statuses, override them in config:

```json
"jira": { "statusMap": { "ERROR": "BLOCKED", "BLOCKED": "TODO" } }
```

**Before relying on it:** publishing has not been tested against your Xray instance. Start with
one test, a Test Execution key you created by hand, and check the result in Jira. Each payload is
also saved as `xray_import.json` in the run's evidence folder, so you can inspect it or import it by
hand.

**Mapping tests:** `config/test_bindings.json` maps Jira test keys to approved definition ids. If
Jira test steps change, review the bound definition. Robo Guide does not parse step text.

## Company access-management platform

No interface is assumed. Robo Guide currently has:
- `NullPlatformClient` (mode `none`): when no platform is configured, the operator records the
  observed result. Those results are marked `OPERATOR_OBSERVED`.
- `SimulatedPlatformClient` (mode `simulated`): a development aid only. Its results are marked
  `SIMULATED` and are never published.

Questions to settle with the platform owners (prime Web or the related services):

1. Which **read-only** interface is approved for test automation: REST API, event stream or
   webhook, an export, or a read-only database view?
2. Is there a test tenant or system, and a dedicated technical account with read-only rights?
3. Event model: which fields identify the **reader/device**, the **credential**, the **decision**
   (granted, denied, unknown, comm error) and the **timestamp** (UTC? device or server clock?)?
4. Typical and worst-case **latency** from card presentation to the event being visible (this sets
   `eventLatencyMs`).
5. How are **device status** (online/offline) and **communication errors** reported?
6. Rate limits, paging, and retention of events.
7. How are LEGIC and MIFARE credentials identified, and can the platform report an *unknown*
   credential's raw identifier?

Once the answers are documented, implement one `PlatformClient` subclass. The architecture doc
lists the steps.
