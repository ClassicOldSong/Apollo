# API

Sunshine/Apollo has a RESTful API which can be used to interact with the service.

Unless otherwise specified, authentication is required for all API calls. You can authenticate using the
`/api/login` endpoint with the admin username and password.

@htmlonly
<script src="api.js"></script>
@endhtmlonly

## POST /api/login
Authenticates the admin account and issues a new `auth` session cookie.

| Field | Type | Required |
| --- | --- | --- |
| username | string | Yes |
| password | string | Yes |

| Description | Input |
| --- | --- |
| Minimal JSON payload | `{"username":"admin","password":"secret"}` |
| curl example | ``curl -k -X POST https://localhost:47990/api/login -H "Content-Type: application/json" -d '{"username":"admin","password":"secret"}'`` |

| Description | Status Code | Example Output |
| --- | --- | --- |
| Happy path | 200 | *(empty body; response includes `Set-Cookie: auth=<token>`)* |
| Unauthorized | 401 | `{"status_code":401,"status":false,"error":"Unauthorized"}` |
| Invalid JSON payload | 500 | *(empty body)*  |


## GET /api/apps
Returns the configured apps list along with host metadata and the currently running app, if any.

| Field | Type | Required |
| --- | --- | --- |
| Cookie: auth | string | Yes |

| Description | Input |
| --- | --- |
| curl example | ``curl -k -b "auth=<token>" https://localhost:47990/api/apps`` |
| PowerShell (excerpt) | ``Invoke-WebRequest -Uri "$BaseUrl/api/apps" -WebSession $session`` |

| Description | Status Code | Example Output |
| --- | --- | --- |
| Happy path | 200 | `{"apps":[{"name":"Steam","uuid":"aaaa-bbbb"}],"current_app":"","host_uuid":"HOST-123","host_name":"Sunshine"}` |
| Unauthorized | 401 | `{"status_code":401,"status":false,"error":"Unauthorized"}` |
| Apps file unreadable | 400 | `{"status_code":400,"status":false,"error":"Bad Request"}` |

## POST /api/apps
Creates a new app entry or updates an existing one in the Sunshine configuration.

| Field | Type | Required |
| --- | --- | --- |
| Cookie: auth | string | Yes |
| name | string | Yes |
| cmd | string | Yes |
| uuid | string | No (required when updating an existing app) |
| output | string | No |
| exclude-global-prep-cmd | boolean | No |
| elevated | boolean | No |
| auto-detach | boolean | No |
| wait-all | boolean | No |
| exit-timeout | number | No |
| prep-cmd | array<object> | No |
| detached | array<string> | No |
| image-path | string (PNG path) | No |

| Description | Input |
| --- | --- |
| Create minimal app | `{"name":"Steam","cmd":"C:\\\\Program Files\\\\Steam\\\\Steam.exe"}` |
| Update existing app | `{"uuid":"aaaa-bbbb","name":"Steam (VR)","cmd":"C:\\\\Program Files\\\\SteamVR.exe"}` |

| Description | Status Code | Example Output |
| --- | --- | --- |
| Happy path | 200 | `{"status":true}` |
| Unauthorized | 401 | `{"status_code":401,"status":false,"error":"Unauthorized"}` |
| Invalid payload (e.g., malformed JSON) | 400 | `{"status_code":400,"status":false,"error":"Bad Request"}` |

## POST /api/apps/close
@copydoc confighttp::closeApp()

## DELETE /api/apps/{index}
@copydoc confighttp::deleteApp()

## GET /api/clients/list
@copydoc confighttp::getClients()

## POST /api/clients/unpair
@copydoc confighttp::unpair()

## POST /api/clients/unpair-all
@copydoc confighttp::unpairAll()

## GET /api/config
@copydoc confighttp::getConfig()

## GET /api/configLocale
@copydoc confighttp::getLocale()

## POST /api/config
@copydoc confighttp::saveConfig()

## POST /api/covers/upload
@copydoc confighttp::uploadCover()

## GET /api/logs
@copydoc confighttp::getLogs()

## POST /api/password
@copydoc confighttp::savePassword()

## POST /api/pin
@copydoc confighttp::savePin()

## POST /api/reset-display-device-persistence
@copydoc confighttp::resetDisplayDevicePersistence()

## POST /api/restart
@copydoc confighttp::restart()

<div class="section_buttons">

| Previous                                    |                                  Next |
|:--------------------------------------------|--------------------------------------:|
| [Performance Tuning](performance_tuning.md) | [Troubleshooting](troubleshooting.md) |

</div>

<details style="display: none;">
  <summary></summary>
  [TOC]
</details>
