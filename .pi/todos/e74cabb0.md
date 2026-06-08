{
"id": "e74cabb0",
"title": "Implement graceful + forceful shutdown in Popen destructor",
"tags": [
"ticket-13",
"destructor",
"feature"
],
"status": "complete",
"created_at": "2026-05-31T04:02:20.827Z",
"assigned_to_session": "019e7c32-4cb0-72b7-a524-24cf66b31a5c"
}

Implement ticket 13: Graceful + forceful shutdown in the Popen destructor.

- Add DestructorPolicy enum and fields to PopenConfig
- Update Popen destructor logic
- Write tests for each policy
- Verify ticket 05 dependency is met
