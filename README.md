# CSV converter on GCloud
Create a docker image for conversion of new binary files to csv. The sequence is:

1. Incoming http POST releases the app.
2. Download all .bin files in the /unprocessed folder on GCP storage
3. Convert these files to csv
4. Upload them back to GCS

The build is based on a public image of GCS-cpp created by Dockage, in Ubuntu. This image builds shared libs of all dependencies (tried to build static, doesn't work - may work with fPIC). So the converter image cannot build a static binary (because we can't link a static binary against .so libs). Therefore the final image is large. Note that both alpine and ubuntu versions work, but the ubuntu version is actually smaller.

## Build and deploy locally
There must be a Google auth keyfile named 'test_auth.json' in the root directory.
 - BUILD: `docker build --tag <SOURCE IMAGE NAME > .`  
 - RUN in shell for testing: `docker run -it -e GOOGLE_APPLICATION_CREDENTIALS="/v/source/test_auth.json" --publish 8080:8080 --name cs --entrypoint bash <SOURCE IMAGE NAME >`  
 - RUN: `docker run -e GOOGLE_APPLICATION_CREDENTIALS="/v/source/test_auth.json" --publish 8080:8080 --name cs --detach <SOURCE IMAGE NAME >`  
 - TEST: `curl -X POST http://localhost:8080`  
 - DELETE: `docker rm -f cs`  

## Deploy container to GCP container registry
`docker tag <SOURCE IMAGE NAME > gcr.io/<PROJECT NAME>/<IMAGE NAME>`  
`docker push gcr.io/<PROJECT NAME>/<IMAGE NAME>`

## Initialise the service
`gcloud run deploy csv-converter --platform managed --region us-east1 --image=gcr.io/<PROJECT NAME>/<IMAGE NAME>`  
Deselecting unauenticated access  

## API use
`curl -H "Authorization: Bearer $(gcloud auth print-identity-token)" https://<API ENDPOINT>`  

## Cleanup
`gcloud container images delete gcr.io/<PROJECT NAME>/<IMAGE NAME>`  