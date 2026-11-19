# Deployment notes

The app is containerized and listens on `0.0.0.0:8080` by default. A platform must
support deploying a Dockerfile and provide a public HTTP ingress. The exact free plans
and deployment flows change, so confirm current pricing and policies before creating
a service. Free services may sleep or have monthly usage limits.

Generic deployment steps:

1. Push this project to a public GitHub repository.
2. Create a new web service on a Docker-capable hosting provider.
3. Connect the repository and select the repository root as the Docker build context.
4. Configure the service's HTTP port as `8080` if the provider requires an explicit port.
5. Set `HTTP_HOST=0.0.0.0`, `HTTP_PORT=8080`, and optionally `HTTP_THREADS=2`.
6. Deploy, then verify `/health` and `/stats` on the public URL.

Do not commit credentials. This project does not require API keys or secrets.

Before using a public deployment, review the server's limitations in the main README.
For a public demo, consider restricting traffic at the hosting layer and avoid treating
this starter implementation as a hardened production server.
