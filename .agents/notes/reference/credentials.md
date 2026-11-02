# Reference: Credentials

This guide describes where the current LX06 client reads credentials and how to keep them out of the repository. Never commit API keys, cookies, SSH passwords, device authentication data, or files copied from a speaker.

## Local credential file

Create `private/credentials.json` on the development Mac. The `private/` directory is ignored by Git. Use this shape and replace the empty strings locally:

```json
{
  "schema_version": 1,
  "ark": { "api_key": "", "model": "" },
  "realtime": { "api_key": "" }
}
```

- `ark.api_key` is the API key for the Ark text model.
- `ark.model` is the model or inference endpoint ID accepted by the Ark console.
- `realtime.api_key` is the API key for the Doubao realtime voice service. It is separate from the Ark key.

The deployed voice process can also receive the realtime key through standard input. The launcher passes it through a FIFO so it is not placed in process arguments or written to an ordinary file. Check the current launch script before adapting this path.

## Validate without printing secrets

Run `tools/Check-LocalCredentials.py` to check whether fields exist and have the expected shape. It prints status flags only; it does not prove that a key is valid or authorized for a cloud service. Do not use `cat`, shell tracing, or verbose command logging on the credential file.

## Realtime service compatibility

The current client implements the Doubao Seed Realtime 3.0 event protocol at the endpoint configured in source. AppID/AccessKey/AppKey credentials for an older service are not interchangeable with an API key for this protocol. Confirm the account's enabled service and authentication scheme before testing a live session.

## Data handling

Keep credentials in `private/` or another access-controlled local secret store. Do not include them in notes, reports, screenshots, terminal transcripts, issue attachments, or chat messages. If a credential was exposed, revoke or rotate it at its provider and replace the local value. Keep user recordings and device readbacks private as well; see [`private/README.md`](../../../private/README.md).
