import requests
import argparse
import json

DISPATCH_SERVER_URL = "http://localhost:8000"

def submit_job(source_url: str, target_codec: str, job_size: float, max_retries: int = 3):
    url = f"{DISPATCH_SERVER_URL}/jobs/"
    payload = {"source_url": source_url, "target_codec": target_codec, "job_size": job_size, "max_retries": max_retries}
    try:
        response = requests.post(url, json=payload)
        response.raise_for_status()
        print("Job submitted successfully:")
        print(json.dumps(response.json(), indent=4))
    except requests.exceptions.RequestException as e:
        print(f"Error submitting job: {e}")

def get_job_status(job_id: str):
    url = f"{DISPATCH_SERVER_URL}/jobs/{job_id}"
    try:
        response = requests.get(url)
        response.raise_for_status()
        print(f"Status for job {job_id}:")
        print(json.dumps(response.json(), indent=4))
    except requests.exceptions.RequestException as e:
        print(f"Error getting job status: {e}")

def get_all_jobs_status():
    url = f"{DISPATCH_SERVER_URL}/jobs/"
    try:
        response = requests.get(url)
        response.raise_for_status()
        print("All Jobs Status:")
        print(json.dumps(response.json(), indent=4))
    except requests.exceptions.RequestException as e:
        print(f"Error getting all jobs status: {e}")

def get_all_engines_status():
    url = f"{DISPATCH_SERVER_URL}/engines/"
    try:
        response = requests.get(url)
        response.raise_for_status()
        print("All Engines Status:")
        print(json.dumps(response.json(), indent=4))
    except requests.exceptions.RequestException as e:
        print(f"Error getting all engines status: {e}")

def download_file(job_id: str, output_path: str):
    # This is a placeholder. In a real scenario, the dispatch server would provide
    # a direct URL to the transcoded file, or the client would request it from a storage service.
    print(f"Downloading for job {job_id} to {output_path} is not yet implemented.")
    print("The dispatch server needs to provide an output_url for the transcoded file.")

def main():
    parser = argparse.ArgumentParser(description="Submission Client for Distributed Transcoding System")
    parser.add_argument("--submit", action="store_true", help="Submit a new transcoding job")
    parser.add_argument("--source_url", type=str, help="Source URL of the video file (with --submit)")
    parser.add_argument("--target_codec", type=str, help="Target codec (e.g., H.264, H.265) (with --submit)")
    parser.add_argument("--job_size", type=float, help="Estimated size/complexity of the job (with --submit)")
    parser.add_argument("--max_retries", type=int, default=3, help="Maximum number of retries for the job (with --submit, default: 3)")
    parser.add_argument("--status", type=str, help="Get status of a job by job ID")
    parser.add_argument("--all_jobs", action="store_true", help="Get status of all jobs")
    parser.add_argument("--all_engines", action="store_true", help="Get status of all transcoding engines")
    parser.add_argument("--download", type=str, help="Download transcoded file by job ID")
    parser.add_argument("--output_path", type=str, help="Path to save the downloaded file (with --download)")

    args = parser.parse_args()

    if args.submit:
        if not args.source_url or not args.target_codec or not args.job_size:
            parser.error("--submit requires --source_url, --target_codec, and --job_size")
        submit_job(args.source_url, args.target_codec, args.job_size, args.max_retries)
    elif args.status:
        get_job_status(args.status)
    elif args.all_jobs:
        get_all_jobs_status()
    elif args.all_engines:
        get_all_engines_status()
    elif args.download:
        if not args.output_path:
            parser.error("--download requires --output_path")
        download_file(args.download, args.output_path)
    else:
        parser.print_help()

if __name__ == "__main__":
    main()
