import requests
import argparse

def submit_job(file_path: str):
    url = "http://127.0.0.1:8000/job/submit"
    with open(file_path, "rb") as f:
        files = {"file": (file_path, f, "video/mp4")}
        response = requests.post(url, files=files)
    
    if response.status_code == 200:
        print("Job submitted successfully!")
        print("Job ID:", response.json()["job_id"])
    else:
        print("Error submitting job:")
        print(response.text)

def get_job_status(job_id: str):
    url = f"http://127.0.0.1:8000/job/{job_id}/status"
    response = requests.get(url)
    if response.status_code == 200:
        print("Job status:")
        print(response.json())
    else:
        print("Error getting job status:")
        print(response.text)

def main():
    parser = argparse.ArgumentParser(description="Submission client for the transcoding service.")
    subparsers = parser.add_subparsers(dest="command")

    # Sub-parser for submitting a job
    parser_submit = subparsers.add_parser("submit", help="Submit a video for transcoding.")
    parser_submit.add_argument("file", help="The path to the video file.")

    # Sub-parser for checking job status
    parser_status = subparsers.add_parser("status", help="Check the status of a transcoding job.")
    parser_status.add_argument("job_id", help="The ID of the job.")

    args = parser.parse_args()

    if args.command == "submit":
        submit_job(args.file)
    elif args.command == "status":
        get_job_status(args.job_id)
    else:
        parser.print_help()

if __name__ == "__main__":
    main()