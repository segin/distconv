from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
from typing import Dict, List, Optional
import uuid
import os
import json

app = FastAPI()

# In-memory storage for jobs and engines (for now)
# In a real application, this would be a database
jobs_db: Dict[str, Dict] = {}
engines_db: Dict[str, Dict] = {}

# Persistent storage for jobs and engines
PERSISTENT_STORAGE_FILE = "dispatch_server_state.json"

class TranscodingJob(BaseModel):
    job_id: str
    source_url: str
    target_codec: str
    status: str = "pending"
    assigned_engine: Optional[str] = None
    output_url: Optional[str] = None
    retries: int = 0

class TranscodingEngine(BaseModel):
    engine_id: str
    status: str = "idle"
    storage_capacity_gb: float
    last_heartbeat: float # Unix timestamp
    benchmark_time: Optional[float] = None # New field for benchmarking

class SubmitJobRequest(BaseModel):
    source_url: str
    target_codec: str

class EngineHeartbeat(BaseModel):
    engine_id: str
    status: str
    storage_capacity_gb: float

class BenchmarkResult(BaseModel):
    engine_id: str
    benchmark_time: float

def load_state():
    if os.path.exists(PERSISTENT_STORAGE_FILE):
        with open(PERSISTENT_STORAGE_FILE, "r") as f:
            state = json.load(f)
            global jobs_db
            global engines_db
            jobs_db = state.get("jobs", {})
            engines_db = state.get("engines", {})
            print(f"Loaded state: jobs={len(jobs_db)}, engines={len(engines_db)}")

def save_state():
    with open(PERSISTENT_STORAGE_FILE, "w") as f:
        json.dump({"jobs": jobs_db, "engines": engines_db}, f, indent=4)
        print("Saved state.")

@app.on_event("startup")
async def startup_event():
    load_state()

@app.on_event("shutdown")
async def shutdown_event():
    save_state()

@app.post("/jobs/")
async def submit_job(request: SubmitJobRequest):
    job_id = str(uuid.uuid4())
    job = TranscodingJob(
        job_id=job_id,
        source_url=request.source_url,
        target_codec=request.target_codec
    )
    jobs_db[job_id] = job.dict()
    save_state()
    return {"message": "Job submitted successfully", "job_id": job_id}

@app.get("/jobs/{job_id}")
async def get_job_status(job_id: str):
    job = jobs_db.get(job_id)
    if not job:
        raise HTTPException(status_code=404, detail="Job not found")
    return job

@app.get("/engines/")
async def list_engines():
    return list(engines_db.values())

@app.post("/engines/heartbeat")
async def engine_heartbeat(heartbeat: EngineHeartbeat):
    engine_id = heartbeat.engine_id
    engines_db[engine_id] = heartbeat.dict()
    save_state()
    return {"message": f"Heartbeat received from engine {engine_id}"}

@app.post("/engines/benchmark_result")
async def engine_benchmark_result(result: BenchmarkResult):
    engine_id = result.engine_id
    if engine_id not in engines_db:
        raise HTTPException(status_code=404, detail="Engine not found")
    engines_db[engine_id]["benchmark_time"] = result.benchmark_time
    save_state()
    return {"message": f"Benchmark result received from engine {engine_id}"}

@app.post("/jobs/{job_id}/complete")
async def complete_job(job_id: str, output_url: str):
    job = jobs_db.get(job_id)
    if not job:
        raise HTTPException(status_code=404, detail="Job not found")
    job["status"] = "completed"
    job["output_url"] = output_url
    save_state()
    return {"message": f"Job {job_id} marked as completed"}

@app.post("/jobs/{job_id}/fail")
async def fail_job(job_id: str, error_message: str):
    job = jobs_db.get(job_id)
    if not job:
        raise HTTPException(status_code=404, detail="Job not found")
    job["status"] = "failed"
    job["error_message"] = error_message
    job["retries"] += 1
    save_state()
    return {"message": f"Job {job_id} marked as failed"}

# Placeholder for job assignment logic (to be implemented later)
@app.post("/assign_job/")
async def assign_job():
    # This function will contain the logic to pick a pending job
    # and assign it to an available engine based on benchmarking data.
    # For now, it's just a placeholder.
    return {"message": "Job assignment logic to be implemented."}

# Placeholder for storage pool configuration (to be implemented later)
@app.get("/storage_pools/")
async def list_storage_pools():
    return {"message": "Storage pool configuration to be implemented."}
