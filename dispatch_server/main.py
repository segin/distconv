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
    job_size: float # New field: e.g., estimated duration, file size, or complexity score
    status: str = "pending"
    assigned_engine: Optional[str] = None
    output_url: Optional[str] = None
    retries: int = 0
    max_retries: int = 3 # New field: maximum number of retries for a job

class TranscodingEngine(BaseModel):
    engine_id: str
    status: str = "idle"
    storage_capacity_gb: float
    last_heartbeat: float # Unix timestamp
    benchmark_time: Optional[float] = None # New field for benchmarking

class SubmitJobRequest(BaseModel):
    source_url: str
    target_codec: str
    job_size: float
    max_retries: int = 3

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
        target_codec=request.target_codec,
        job_size=request.job_size,
        max_retries=request.max_retries
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

@app.get("/jobs/")
async def list_all_jobs():
    return list(jobs_db.values())

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

    job["retries"] += 1
    if job["retries"] <= job["max_retries"]:
        job["status"] = "pending"  # Re-queue the job for another attempt
        job["error_message"] = error_message # Store the last error message
        print(f"Job {job_id} failed, re-queuing. Retries: {job["retries"]}/{job["max_retries"]}")
    else:
        job["status"] = "failed_permanently"
        job["error_message"] = f"Job failed permanently after {job["retries"]} attempts: {error_message}"
        print(f"Job {job_id} failed permanently.")
    save_state()
    return {"message": f"Job {job_id} status updated to {job["status"]}"}

@app.post("/assign_job/")
async def assign_job():
    # Find a pending job
    pending_job = None
    for job_id, job_data in jobs_db.items():
        if job_data["status"] == "pending":
            pending_job = job_data
            break

    if not pending_job:
        return {"message": "No pending jobs to assign."}

    # Find an idle engine with benchmarking data
    available_engines = []
    for engine_id, engine_data in engines_db.items():
        if engine_data["status"] == "idle" and engine_data.get("benchmark_time") is not None:
            available_engines.append(engine_data)

    if not available_engines:
        return {"message": "No idle engines with benchmarking data available."}

    # Sort engines by benchmark_time (faster engines first)
    # For simplicity, we'll assign smaller jobs to slower systems and bigger jobs to faster systems.
    # This is a basic implementation and can be improved with more sophisticated scheduling algorithms.
    available_engines.sort(key=lambda x: x["benchmark_time"])

    # Determine job size category (e.g., small, medium, large)
    # For now, a simple threshold. This would be more dynamic in a real system.
    job_size = pending_job.get("job_size", 0) # Default to 0 if not provided
    
    selected_engine = None
    if job_size < 50: # Example threshold for "small" jobs
        # Assign small jobs to slower engines (larger benchmark_time)
        selected_engine = available_engines[-1] # Last one is slowest
    else:
        # Assign larger jobs to faster engines (smaller benchmark_time)
        selected_engine = available_engines[0] # First one is fastest

    if not selected_engine:
        return {"message": "Could not find a suitable engine for the job."}

    # Assign the job
    pending_job["status"] = "assigned"
    pending_job["assigned_engine"] = selected_engine["engine_id"]
    jobs_db[pending_job["job_id"]] = pending_job

    selected_engine["status"] = "busy"
    engines_db[selected_engine["engine_id"]] = selected_engine
    
    save_state()

    return {
        "message": "Job assigned successfully",
        "job_id": pending_job["job_id"],
        "assigned_engine": selected_engine["engine_id"]
    }

# Placeholder for storage pool configuration (to be implemented later)
@app.get("/storage_pools/")
async def list_storage_pools():
    return {"message": "Storage pool configuration to be implemented."}
