# Squiggle

This is an experiment DSL/language for making probabilistic estimates. The full story can be found [here](https://www.lesswrong.com/s/rDe8QE5NvXcZYzgZ3). 

This monorepo has several packages that can be used for various purposes. All 
the packages can be found in `packages`.

- `@quri/squiggle-lang` in `packages/squiggle-lang` contains the core language, particularly
an interface to parse squiggle expressions and return descriptions of distributions
or results.
- `@quri/squiggle-components` in `packages/components` contains React components that
can be passed squiggle strings as props, and return a presentation of the result
of the calculation.
- `@quri/playground` in `packages/playground` contains a website for a playground
for squiggle. This website is hosted at `playground.squiggle-language.com`
- `@quri/squiggle-website` in `packages/website` The main descriptive website for squiggle,
it is hosted at `squiggle-language.com`.

The playground depends on the components library which then depends on the language.
This means that if you wish to work on the components library, you will need
to build and bundle the language, and for the playground to work, you will need to build and bundle
the components library and the playground.

# Develop

For any project in the repo, begin by running `yarn` in the top level (TODO: is this true?) 

``` sh
yarn
```

See `packages/*/README.md` to work with whatever project you're interested in. 

## `codium` for `rescript` 

If you have `nix` installed with `flakes` enabled, you can build a `codium` in this repo for `rescript` development, if you don't want to pollute your machine's global editor with another mode/extension. 

``` sh
nix develop
codium 
```

# Contributing
See `CONTRIBUTING.md`. 
