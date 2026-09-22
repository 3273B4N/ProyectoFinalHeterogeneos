# NEAT

This repo is a library for evolving neural networks with NEAT written in C++.

⚠️ WARNING: This project contains several issues and will not be updated. Check [PNEATM](https://github.com/titofra/PNEATM) for my own neural networks which is based on this repo for more.

<p align="center">
	<img src="https://github.com/titofra/NEAT/raw/main/resources/network.png" width="700">
</p>

## What
[Neuroevolution of augmenting topologies (NEAT)](https://en.wikipedia.org/wiki/Neuroevolution_of_augmenting_topologies) is a machine learning technique used for evolving artificial neural networks. NEAT uses a genetic algorithm to optimize the topology, weights, and activation functions of neural networks to solve a given problem. It allows the networks to evolve and adapt over time by adding or removing neurons and connections. NEAT is often used for complex tasks such as game playing and robotics, where traditional hand-designed neural networks may not be effective.

## Examples
A skeleton project can be found in [/examples/template/](https://github.com/titofra/NEAT/tree/main/examples/template). Moreover, a Snake AI powered by NEAT is available in [/examples/snake/](https://github.com/titofra/NEAT/tree/main/examples/snake).

[resources_snakeNEAT.webm](https://user-images.githubusercontent.com/120715525/233745593-d2044124-56b4-4479-91bd-f73eb2f2e5ab.webm)

## Modifications

While testing the Snake example, the program occasionally crashed with a
segmentation fault after several hundred generations. As the author notes above,
this repository is no longer maintained, so the bug was fixed locally in
`src/genome.cpp`.

Cause: when updating node layers after a mutation, `updateLayersRec` always
set `layer = parent + 1`. If a node had another, deeper parent, its layer could
decrease, leaving connections pointing backwards. This could form cycles in a
non-recurrent network, making `updateLayersRec` recurse indefinitely until the
stack overflowed. Since mutations are random, the crash did not happen on every run.

Fix: layers can now only increase, and a disabled connection is not
re-enabled if it would point backwards. The changes are marked with `// Fix:`
comments in `src/genome.cpp`.

Impact on the algorithm: NEAT itself was not changed. Evaluation, speciation,
crossover and mutation work the same way. The fix only enforces a rule that
non-recurrent networks were already supposed to follow (information flows forward
only), so the only difference is that networks that were previously evaluated
incorrectly are now evaluated correctly.

## Credits
Based on the work of:
- [Kenneth Stanley](https://www.cs.ucf.edu/~kstanley/neat.html)
- [Neat AI](https://www.youtube.com/watch?v=3nbvrrdymF0&list=PLnICFpQDyZRFqjdtcTjshOb1IJqns6h6w)
- [K.O. Stanley and R. Miikkulainen](http://nn.cs.utexas.edu/downloads/papers/stanley.ec02.pdf)
- [Kenneth O. Stanley, Bobby D. Bryant, Risto Miikkulainen](http://nn.cs.utexas.edu/downloads/papers/stanley.ieeetec05.pdf)
