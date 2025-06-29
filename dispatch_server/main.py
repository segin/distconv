import asyncio
from fastapi import FastAPI, UploadFile, File, Form
from pydantic import BaseModel
from typing import List, Dict, Optional
import uuid

app = FastAPI()

class Engine(BaseModel):
    id: str
    status: str  # idle, transcoding
    storage_capacity: int # in MB
    last_heartbeat: float

class Job(BaseModel):
    id: str
    status: str  # pending, assigned, completed, failed
    source_file_path: str
    output_file_path: Optional[str] = None
    engine_id: Optional[str] = None

# In-memory data stores
engines: Dict[str, Engine] = {}
jobs: Dict[str, Job] = {}
job_queue: asyncio.Queue = asyncio.Queue()

@app.post("/engine/register")
async def register_engine(engine_id: str = Form(...), storage_capacity: int = Form(...)):
    if engine_id not in engines:
        engines[engine_id] = Engine(id=engine_id, status="idle", storage_capacity=storage_capacity, last_heartbeat=asyncio.get_event_loop().time())
        return {"message": "Engine registered successfully"}
    return {"message": "Engine already registered"}

@app.post("/engine/heartbeat")
async def engine_heartbeat(engine_id: str = Form(...), status: str = Form(...)):
    if engine_id in engines:
        engines[engine_id].status = status
        engines[engine_id].last_heartbeat = asyncio.get_event_loop().time()
        return {"message": "Heartbeat received"}
    return {"error": "Engine not found"}, 404

@app.post("/job/submit")
async def submit_job(file: UploadFile = File(...)):
    job_id = str(uuid.uuid4())
    source_file_path = f"uploads/{job_id}_{file.filename}"
    with open(source_file_path, "wb") as buffer:
        buffer.write(await file.read())

    job = Job(id=job_id, status="pending", source_file_path=source_file_path)
    jobs[job_id] = job
    await job_queue.put(job)
    return {"job_id": job_id}

@app.get("/job/{job_id}/status")
async def get_job_status(job_id: str):
    if job_id in jobs:
        return jobs[job_id]
    return {"error": "Job not found"}, 404

@app.get("/engines", response_model=List[Engine])
async def list_engines():
    return list(engines.values())

async def job_scheduler():
    while True:
        job = await job_queue.get()
        
        available_engine = None
        while not available_engine:
            for engine in engines.values():
                if engine.status == "idle":
                    available_engine = engine
                    break
            await asyncio.sleep(1) # wait for an engine to become available

        job.status = "assigned"
        job.engine_id = available_engine.id
        available_engine.status = "transcoding"
        
        # Assign job to engine
        # In a real implementation, we would have a more robust way of communicating with the engine
        # For now, we assume the engine has an endpoint to receive jobs
        print(f"Assigning job {job.id} to engine {available_engine.id}")
        # This is a placeholder for the actual job assignment
        # In a real implementation, we would make a request to the engine's API
        # For example:
        # requests.post(f"http://{available_engine.address}/job/run", json=job.dict())

@app.get("/job/next")
async def get_next_job():
    try:
        job = job_queue.get_nowait()
        return job
    except asyncio.QueueEmpty:
        return {}

@app.post("/job/complete")
async def job_complete(job_id: str = Form(...), output_file_path: str = Form(...)):
    if job_id in jobs:
        jobs[job_id].status = "completed"
        jobs[job_id].output_file_path = output_file_path
        engine_id = jobs[job_id].engine_id
        if engine_id in engines:
            engines[engine_id].status = "idle"
        return {"message": "Job marked as complete"}
    return {"error": "Job not found"}, 404


@app.on_event("startup")
async def startup_event():
    # Create uploads directory
    import os
    if not os.path.exists("uploads"):
        os.makedirs("uploads")
    asyncio.create_task(job_scheduler())

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)