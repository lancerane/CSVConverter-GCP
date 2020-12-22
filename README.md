# CSV converter on GCloud
Create a docker image for conversion of new binary files to csv. The sequence is:

1. Incoming http POST releases the app.
2. Download all .bin files in the /unprocessed folder on GCP storage
3. Convert these files to csv
4. Upload them back to GCS

The build is based on a public image of GCS-cpp created by Dockage, in Ubuntu. This image builds shared libs of all dependencies (tried to build static, doesn't work - may work with fPIC). So the converter image cannot build a static binary (because we can't link a static binary against .so libs). Therefore the final image is large. Note that both alpine and ubuntu versions work, but the ubuntu version is actually smaller.

## Build and deploy locally
BUILD: `docker build --tag csv_converter_gcp .`
RUN in shell for testing: `docker run -it -e GOOGLE_APPLICATION_CREDENTIALS="/v/source/test_auth.json" --publish 8080:8080 --name cs --entrypoint bash csv_converter_gcp`
RUN: `docker run -e GOOGLE_APPLICATION_CREDENTIALS="/v/source/test_auth.json" --publish 8080:8080 --name cs --detach csv_converter_gcp`
TEST: `curl -X POST http://localhost:8080`
DELETE: `docker rm -f cs`

## Deploy container to GCP container registry
`docker tag csv_converter_gcp gcr.io/k8s-test-153-project/csv_converter`
`docker push gcr.io/k8s-test-153-project/csv_converter`

## Initialise the service
`gcloud run deploy csv-converter --platform managed --region us-east1 --image=gcr.io/k8s-test-153-project/csv_converter`
Deselecting unauenticated access

## API use
`curl -H "Authorization: Bearer $(gcloud auth print-identity-token)" https://csv-converter-b73p6ral2q-ue.a.run.app`

## Cleanup
`gcloud container images delete gcr.io/k8s-test-153-project/csv_converter`