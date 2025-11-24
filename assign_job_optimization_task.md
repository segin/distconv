---
description: Optimize /assign_job endpoint and add priority, retry, cancel features
---

# Assign Job Optimization Task

## Goals
- Replace the current linear scan of all jobs in the `/assign_job` endpoint with an efficient query or dedicated queue.
- Ensure jobs have a `priority` field that can be set at submission time and is respected during assignment.
- Add a manual retry endpoint (`POST /jobs/{id}/retry`).
- Add a cancel endpoint (`POST /jobs/{id}/cancel`).

## Steps
1. **Repository Enhancements**
   - Add a method `get_next_pending_job_by_priority(const std::vector<std::string>& capable_engines)` to `IJobRepository` that returns the highest‑priority pending job that matches engine capabilities.
   - Implement this method for both SQLite and in‑memory repositories using an appropriate SQL query (ORDER BY priority DESC, created_at ASC) or in‑memory sorting.
2. **Job Submission Update**
   - Extend the job submission handler to accept a `priority` field (integer) and store it in the job JSON.
3. **Assign Job Handler Refactor**
   - Modify `JobAssignmentHandler` to call the new repository method instead of iterating `jobs_db`.
   - Remove the old `find_pending_job` helper.
4. **Retry & Cancel Endpoints**
   - Ensure the previously added `JobRetryHandler` and `JobCancelHandler` are registered (already done) and use the repository to update job state.
5. **Tests**
   - Add unit tests for the new repository method, priority handling, and the retry/cancel endpoints.
6. **Documentation**
   - Update `SUGGESTIONS.md` to mark the related checklist items as completed.

## Verification
- Run the new test suite and confirm all tests pass.
- Manually exercise the `/assign_job` endpoint and verify it returns the highest‑priority job.
- Verify that retry and cancel endpoints correctly change job state.

## Timeline
- Repository changes: 2 hours
- Handler refactor: 1 hour
- Tests and verification: 1 hour
- Documentation update: 30 minutes

---
