# Serialization Sanitizer

This project uses a combination of compile-time code generation with a Clang
frontend tooling pass and a runtime sanitizer to discover missing members during
serialization of classes.

## Building

- Get `docker` and `docker-compose`. Then, to build `cd` into the repository
directory after cloning from git.

- Fetch the latest VT develop image.

```shell
docker-compose pull vt-base
```

This will download the latest image that was uploaded to Docker Hub, which
automatically updates from a GitHub workflow in the DARMA/*vt* repository when
code is merged into *vt*'s develop branch and it builds correctly and passes all
tests.

- *Optional*: fetch the sanitizer base image (Clang frontend tooling and
  associated dependencies) if the clang compiler version exists. If it doesn't
  exist for your preferred version, build it yourself (next step).

```shell
docker-compose pull sanitizer-base
```

- Bring images up on `docker-compose`:

```shell
docker-compose up
```

- Launch the sanitizer container interactively:

```shell
docker-compose run sanitizer-base
```
