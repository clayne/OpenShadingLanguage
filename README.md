<div align="center">
  <img src="ASWF/logos/openshadinglanguage-stacked-Shaded.png">

  <!-- <H1>Open Shading Language</H1> -->
</div>

[![Open Shading Language Reel 2020](http://img.youtube.com/vi/nmwMz1YnGPA/0.jpg)](https://www.youtube.com/watch?v=nmwMz1YnGPA "Open Shading Language Reel 2020")

[![Build Status](https://github.com/AcademySoftwareFoundation/OpenShadingLanguage/workflows/CI/badge.svg)](https://github.com/AcademySoftwareFoundation/OpenShadingLanguage/actions)
[![License](https://img.shields.io/badge/license-BSD%203--Clause-blue.svg?style=flat-square)](https://github.com/AcademySoftwareFoundation/OpenShadingLanguage/LICENSE.md)
[![CII Best Practices](https://bestpractices.coreinfrastructure.org/projects/3061/badge)](https://bestpractices.coreinfrastructure.org/projects/3061)
[![Documentation Status](https://readthedocs.org/projects/open-shading-language/badge/?version=main)](https://open-shading-language.readthedocs.io/en/main/?badge=main)


Introduction
------------

Welcome to Open Shading Language!

Open Shading Language (OSL) is a small but rich language for
programmable shading in advanced renderers and other applications, ideal
for describing materials, lights, displacement, and pattern generation.

OSL was originally developed by Sony Pictures Imageworks for use in its in-
house renderer used for feature film animation and visual effects, released
as open source so it could be used by other visual effects and animation
studios and rendering software vendors. Now it's the de facto standard
shading language for VFX and animated features, used across the industry in
many commercial and studio- proprietary renderers. Because of this, the work
on OSL received an Academy Award for Technical Achievement in 2017.

OSL is robust and production-proven, and has been used in films as diverse
as "The Amazing Spider-Man," "Hotel Transylvania," "Edge of Tomorrow", "Ant
Man", "Finding Dory," and many more. OSL support is in most leading
renderers used for high-end VFX and animation work. For a full list of films
and products, see the [filmography](#where-osl-has-been-used).

The OSL code is distributed under the
["New/3-clause BSD" license](https://github.com/AcademySoftwareFoundation/OpenShadingLanguage/LICENSE.md),
and the documentation under the [Creative Commons Attribution 4.0 International
License](https://creativecommons.org/licenses/by/4.0/).  In short, you are
free to use OSL in your own applications, whether they are free or
commercial, open or proprietary, as well as to modify the OSL code and
documentation as you desire, provided that you retain the original copyright
notices as described in the license.


How OSL is different
--------------------

OSL has syntax similar to C, as well as other shading languages.
However, it is specifically designed for advanced rendering algorithms
and has features such as radiance closures, BSDFs, and deferred ray
tracing as first-class concepts.

OSL has several unique characteristics not found in other shading
languages (certainly not all together).  Here are some things you will
find are different in OSL compared to other languages:

* Surface and volume shaders compute radiance closures, not final colors.

  OSL's surface and volume shaders compute an explicit symbolic
  description, called a "closure", of the way a surface or volume
  scatters light, in units of radiance.  These radiance closures may be
  evaluated in particular directions, sampled to find important
  directions, or saved for later evaluation and re-evaluation.
  This new approach is ideal for a physically-based renderer that
  supports ray tracing and global illumination.

  In contrast, other shading languages usually compute just a surface
  color as visible from a particular direction.  These old shaders are
  "black boxes" that a renderer can do little with but execute to find
  this one piece of information (for example, there is no effective way
  to discover from them which directions are important to sample).
  Furthermore, the physical units of lights and surfaces are often
  underspecified, making it very difficult to ensure that shaders are
  behaving in a physically correct manner.

* Surface and volume shaders do not loop over lights or shoot rays.

  There are no "light loops" or explicitly traced illumination rays in
  OSL surface shaders.  Instead, surface shaders compute a radiance
  closure describing how the surface scatters light, and a part of the
  renderer called an "integrator" evaluates the closures for a
  particular set of light sources and determines in which directions
  rays should be traced.  Effects that would ordinarily require explicit
  ray tracing, such as reflection and refraction, are simply part of the
  radiance closure and look like any other BSDF.

  Advantages of this approach include that integration and sampling may
  be batched or re-ordered to increase ray coherence; a "ray budget" can
  be allocated to optimally sample the BSDF; the closures may be used by
  for bidirectional ray tracing or Metropolis light transport; and the
  closures may be rapidly re-evaluated with new lighting without having
  to re-run the shaders.

* Surface and light shaders are the same thing.

  OSL does not have a separate kind of shader for light sources.  Lights
  are simply surfaces that are emissive, and all lights are area lights.

* Transparency is just another kind of illumination.

  You don't need to explicitly set transparency/opacity variables in the
  shader.  Transparency is just another way for light to interact with a
  surface, and is included in the main radiance closure computed by a
  surface shader.

* Renderer outputs (AOV's) may be specified using "light path expressions."

  Sometimes it is desirable to output images containing individual
  lighting components such as specular, diffuse, reflection, individual
  lights, etc.  In other languages, this is usually accomplished by
  adding a plethora of "output variables" to the shaders that collect
  these individual quantities.

  OSL shaders need not be cluttered with any code or output variables to
  accomplish this.  Instead, there is a regular-expression-based
  notation for describing which light paths should contribute to which
  outputs.  This is all done on the renderer side (though supported by
  the OSL implementation).  If you desire a new output, there is no need
  to modify the shaders at all; you only need to tell the renderer the
  new light path expression.

* Shaders are organized into networks.

  OSL shaders are not monolithic, but rather can be organized into
  networks of shaders (sometimes called a shader group, graph, or DAG),
  with named outputs of some nodes being connected to named inputs of
  other nodes within the network.  These connections may be done
  dynamically at render time, and do not affect compilation of
  individual shader nodes.  Furthermore, the individual nodes are
  evaluated lazily, only when their outputs are "pulled" from the later
  nodes that depend on them (shader writers may remain blissfully
  unaware of these details, and write shaders as if everything is
  evaluated normally).

* Arbitrary derivatives without grids or extra shading points.

  In OSL, you can take derivatives of any computed quantity in a shader,
  and use arbitrary quantities as texture coordinates and expect correct
  filtering.  This does not require that shaded points be arranged in a
  rectangular grid, or have any particular connectivity, or that any
  "extra points" be shaded.  This is because derivatives are not
  computed by finite differences with neighboring points, but rather by
  "automatic differentiation", computing partial differentials for the
  variables that lead to derivatives, without any intervention required
  by the shader writer.

* OSL optimizes aggressively at render time

  OSL uses the LLVM compiler framework to translate shader networks into
  machine code on the fly (just in time, or "JIT"), and in the process
  heavily optimizes shaders and networks with full knowledge of the
  shader parameters and other runtime values that could not have been
  known when the shaders were compiled from source code.  As a result,
  we are seeing our OSL shading networks execute 25% faster than the
  equivalent shaders hand-crafted in C!  (That's how our old shaders
  worked in our renderer.)



What OSL consists of
--------------------

The OSL open source distribution consists of the following components:

* oslc, a standalone compiler that translates OSL source code into
  an assembly-like intermediate code (in the form of .oso files).

* liboslc, a library that implements the OSLCompiler class, which
  contains the guts of the shader compiler, in case anybody needs to
  embed it into other applications and does not desire for the compiler
  to be a separate executable.

* liboslquery, a library that implements the OSLQuery class, which
  allows applications to query information about compiled shaders,
  including a full list of its parameters, their types, and any metadata
  associated with them.

* oslinfo, a command-line program that uses liboslquery to print to the
  console all the relevant information about a shader and its parameters.

* liboslexec, a library that implements the ShadingSystem class, which
  allows compiled shaders to be executed within an application.
  Currently, it uses LLVM to JIT compile the shader bytecode to x86
  instructions.

* testshade, a program that lets you execute a shader (or connected
  shader network) on a rectangular array of points, and save any of its
  outputs as images.  This allows for verification of shaders (and the
  shading system) without needing to be integrated into a fully
  functional renderer, and is the basis for most of our testsuite
  verification.  Along with testrender, testshade is a good example
  of how to call the OSL libraries.

* testrender, a tiny ray-tracing renderer that uses OSL for shading.
  Features are very minimal (only spheres are permitted at this time)
  and there has been no attention to performance, but it demonstrates how
  the OSL libraries may be integrated into a working renderer, what
  interfaces the renderer needs to supply, and how the BSDFs/radiance
  closures should be evaluated and integrated (including with multiple
  importance sampling).

* A few sample shaders.

* Documentation -- at this point consisting of the OSL language
  specification (useful for shader writers), but in the future will have
  detailed documentation about how to integrate the OSL libraries into
  renderers.



Where OSL has been used
-----------------------

*This list only contains films or products whose OSL use is stated or can be
inferred from public sources, or that we've been told is ok to list here. If
an OSL-using project is missing and it's not a secret, just email the OSL
project leader or submit a PR with edits to this file.*

### Renderers and other production tools with OSL support

(In approximate order of adding OSL support)

* [Sony Pictures Imageworks: in-house "Arnold" renderer](https://dl.acm.org/doi/10.1145/3180495)
* [Blender/Cycles](https://docs.blender.org/manual/en/dev/render/shader_nodes/osl.html)
* [Chaos Group: V-Ray](https://www.chaosgroup.com/)
* [Pixar: PhotoRealistic RenderMan RIS](https://renderman.pixar.com)
* Isotropix: [Clarisse iFX](https://www.isotropix.com/products/clarisse/ifx) / [Clarisse BUiLDER](https://www.isotropix.com/products/clarisse/builder) / [Angie](https://www.isotropix.com/products/angie)
* [Autodesk Beast](http://www.autodesk.com/products/beast/overview)
* [Appleseed](http://appleseedhq.net)
* [Animal Logic: Glimpse renderer](https://www.fxguide.com/featured/a-glimpse-at-animal-logic/)
* [Image Engine: Gaffer](http://www.gafferhq.org/) (for expressions and deformers)
* [DNA Research: 3Delight](http://www.3delight.com/)
* Ubisoft motion picture group's proprietary renderer
* [Autodesk/SolidAngle: Arnold](https://www.solidangle.com/arnold/)
* [Autodesk: 3DS Max 2019](https://help.autodesk.com/view/3DSMAX/2019/ENU/?guid=__developer_3ds_max_sdk_features_rendering_osl_html)
* [Maxon: Redshift](https://www.maxon.net/en/redshift)

### Significant work using OSL, grouped by year of release date:

(Here we are considering "significant work" to mean a feature film released
theatrically or on a major streaming platform, TV/streaming series heavily
featuring visual effects or animation, or short films that have won or been
nominated for major awards.)

* **(2012)**
  Men in Black 3, The Amazing Spider-Man, Hotel Transylvania
* **(2013)**
  Oz the Great and Powerful, Smurfs 2, Cloudy With a Chance of Meatballs 2
* **(2014)**
  The Amazing Spider-Man 2, Blended, Edge of Tomorrow, 22 Jump Street,
  Guardians of the Galaxy, Fury,
  The Hunger Games: Mockingjay - Part 1, Exodus: Gods and Kings,
  The Interview
* **(2015)**
  American Sniper,
  Insurgent,
  Avengers Age of Ultron,
  Ant Man,
  Pixels,
  Mission Impossible: Rogue Nation,
  Hotel Transylvania 2,
  Bridge of Spies,
  James Bond: Spectre,
  The Hunger Games: Mockingjay - Part 2,
  Concussion
* **(2016)**
  Allegiant,
  Batman vs Superman: Dawn of Justice,
  The Huntsman,
  Angry Birds Movie,
  Alice Through the Looking Glass,
  Captain America: Civil War,
  Finding Dory,
  Piper,
  Independence Day: Resurgence,
  Ghostbusters,
  Star Trek Beyond,
  Suicide Squad,
  Kubo and the Two Strings,
  Kingsglaive: Final Fantasy XV,
  Storks,
  Miss Peregrine's Home for Peculiar Children,
  Fantastic Beasts and Where to Find Them,
  Assassin's Creed
* **(2017)**
  Lego Batman,
  The Great Wall,
  A Cure for Wellness,
  Logan,
  Power Rangers,
  Life,
  Smurfs: The Lost Village,
  The Fate of the Furious,
  Alien Covenant,
  Guardians of the Galaxy 2,
  The Mummy,
  Wonder Woman,
  Cars 3,
  Baby Driver,
  Spider-Man: Homecoming,
  Dunkirk,
  The Emoji Movie,
  Detroit,
  Kingsman: The Golden Circle,
  Lego Ninjago Movie,
  Blade Runner 2049,
  Geostorm,
  Coco,
  Justice League,
  Thor: Ragnarok
* **(2018)**
  Peter Rabbit,
  Black Panther,
  Annihilation,
  Red Sparrow,
  Pacific Rim Uprising,
  Avengers Infinity War,
  Deadpool 2,
  Incredibles 2,
  Jurassic World: Fallen Kingdom,
  Hotel Transylvania 3: Summer Vacation,
  Ant Man and the Wasp,
  Skyscraper,
  Mission Impossible: Fallout,
  The Meg,
  Kin,
  Smallfoot,
  Alpha,
  Venom,
  First Man,
  Bad Times at the El Royale,
  Fantastic Beasts: The Crimes of Grindelwald,
  Bohemian Rhapsody,
  Holmes and Watson,
  Spider-Man: Into the Spider-Verse
* **(2019)**
  The Kid Who Would Be King,
  Alita: Battle Angel,
  Lego Movie 2,
  Lucky 13 (Love, Death, and Robots),
  Captain Marvel,
  Triple Frontier,
  Avengers: Endgame,
  Pokémon Detective Pikachu,
  Godzilla: King of Monsters,
  Rim of the World,
  John Wick 3 Parabellum,
  Men in Black International,
  Toy Story 4,
  Spider-Man: Far From Home,
  Hobbs & Shaw,
  Angry Birds 2,
  The Art of Racing in the Rain,
  Secret Life of Pets,
  The Mandalorian (S1),
  The Dark Crystal: Age of Resistance,
  The King,
  Jumanji: The Next Level,
  Richard Jewell,
  Game of Thrones (S8),
  Lost in Space (S1),
  Togo,
  Missing Link
* **(2020)**
  Underwater,
  Birds of Prey,
  Onward,
  Bloodshot,
  Greyhound,
  The Old Guard,
  Mulan,
  Tenet,
  The New Mutants,
  Artemis Fowl,
  The Eight Hundred,
  Over the Moon,
  Wonder Woman 1984,
  Soul,
  The Mandalorian (S2),
  The Boys (S2),
  Umbrella Academy (S2)
* **(2021)**
  Chaos Walking,
  Peter Rabbit 2: The Runaway,
  The Falcon and the Winder Soldier,
  Secret Magic Control Agency,
  Zack Snyder's Justice League,
  The Mitchells vs the Machines,
  Jupiter's Legacy,
  Luca,
  F9,
  Vivo,
  Jungle Cruise,
  Cinderella,
  Dune,
  No Time To Die,
  Ron's Gone Wrong,
  Venom: Let There Be Carnage,
  Last Night in Soho,
  Ghostbusters: Afterlife,
  Spider-Man 3: No Way Home,
  Matrix Resurrections,
  Sing 2
* **(2022)**
  Hotel Transylvania 4: Transformania,
  Death on the Nile,
  Uncharted,
  Turning Red,
  The Adam Project,
  Doctor Strange in the Multiverse of Madness,
  Love, Death, and Robots: In Vaulted Halls Entombed,
  Jurassic World: Dominion,
  Lightyear,
  Thor: Love and Thunder,
  The Sea Beast,
  DC League of Super Pets,
  Minions: Rise of Gru,
  Bullet Train,
  Slumberland,
  Glass Onion,
  Puss in Boots: The Last Wish,
  Archive 81 (series),
  Moon Night (series),
  Obi-Wan Kenobi (series),
  The Boys (S3),
  Andor (S1),
  Troll,
  She-Hulk (series)
* **(2023)**
  Ant-Man and the Wasp: Quantumania,
  The Mandalorian S3,
  The Magician's Elephant,
  Super Mario Bros Film,
  Guardians of the Galaxy 3,
  Spider-Man: Across the Spider-Verse,
  Elemental,
  Indiana Jones and the Dial of Destiny,
  The Marvels,
  Leo,
  The Boys (S4),
  The Moon
* **(2024)**
  Ghostbusters: Frozen Empire,
  Inside Out 2,
  Red One,
  Dream Productions (series)
* **(2025 / upcoming)**
  Win or Lose (series),
  In Your Dreams,
  A Minecraft Movie,
  ...


Building and Installation
-------------------------

Please read the [INSTALL.md](INSTALL.md) file for detailed instructions on
how to build and install OSL.


Documentation
-------------

The OSL language specification can be found at
[src/doc/osl-languagespec.pdf](src/doc/osl-languagespec.pdf) (in a source
distribution) or in the share/doc/OSL/osl-languagespec.pdf file of an
installed binary distribution.

[OSL Documentation on ReadTheDocs](https://docs.openshadinglanguage.org)
This will be the future documentation. It's probably as complete as the PDF,
but it needs some proofreading, so the PDF is still considered the
authoritative source for now. But some time soon, the old PDF specification
will be deprecated in favor of
this online documentation.
<!-- is the best place to start if you are interested in how to use OSL,
its APIs. -->
There is also a [PDF
version](https://readthedocs.org/projects/open-shading-language/downloads/pdf/latest/).

For those interested in learning to program OSL shaders there is the
[Siggraph 2024 Course](https://dl.acm.org/doi/proceedings/10.1145/3664475), 
[Shader Writing in Open Shading Language](https://dl.acm.org/doi/10.1145/3664475.3664534)
and its companion 
[Supplementary Materials](https://github.com/LaikaStudios/OSLShaderWriting),
which use RenderMan<sup>&reg;</sup> in the examples but which are primarily about writing OSL shaders.

There is also the [Shader Writing in Open Shading Language book](https://www.routledge.com/Shader-Writing-in-Open-Shading-Language-with-RenderManr-Examples/Prater/p/book/9781032421100).
This covers all the material from the Siggraph course but in far greater detail. It also includes much more information about how OSL operates and covers a much broader range of shader examples.


Contact & reporting problems
----------------------------

Simple "how do I...", "I'm having trouble", or "is this a bug" questions are
best asked on the [osl-dev developer mail list](https://lists.aswf.io/g/osl-dev).
That's where the most people will see it and potentially be able to answer
your question quickly (more so than a GH "issue"). For quick questions, you
could also try the [ASWF Slack](https://slack.aswf.io) `#openshadinglanguage`
channel.

Bugs, build problems, and discovered vulnerabilities that you are relatively
certain is a legit problem in the code, and for which you can give clear
instructions for how to reproduce, should be [reported as
issues](https://github.com/AcademySoftwareFoundation/OpenShadingLanguage/issues).

If you think you've found a potential vulnerability in OSL, please
confidentially report it by emailing the project administrators at
[security@openshadinglanguage.org](security@openshadinglanguage.org).

If any other issue requires confidentiality that precludes a public question
or issue, you may contact the project administrator privately at
[lg@larrygritz.com](lg@larrygritz.com).


Contributing
------------

OSL welcomes code contributions, and nearly 50 people have done so over the
years. We take code contributions via the usual GitHub pull request (PR)
mechanism. Please see [CONTRIBUTING](CONTRIBUTING.md) for detailed
instructions.


Contacts, Links, and References
-------------------------------

[OSL GitHub page](https://github.com/AcademySoftwareFoundation/OpenShadingLanguage)

[Read or subscribe to the OSL development mail list](https://lists.aswf.io/g/osl-dev)

[Most recent PDF of the OSL language specification](https://github.com/AcademySoftwareFoundation/OpenShadingLanguage/blob/main/src/doc/osl-languagespec.pdf
)

[OSL home page](http://openshadinglanguage.org)




Credits
-------

The current project leadership is documented in the
[Governance](GOVERNANCE.md) file.

Many people have contributed features, bug fixes, and other changes to OSL
over the years: Steve Agland, Shane Ambler, Martijn Berger, Farchad
Bidgolirad, Nicholas Bishop, Curtis Black, Rasmus Bonnedal, Solomon Boulos,
Stefan Bruens, Stefan Büttner, Matthaus G. Chajdas, Clark Chen, Mehdi
Chinoune, Alejandro Conty, Damien Courtois, Dieter De Baets, Thomas Dinges,
Daniel Dresser, Mads Drøschler, Peter Ellerington, Luke Emrose, Louis Feng,
Mark Final, Henri Fousse, Stephen Friedman, Syoyo Fujita, Tim Grant, Larry
Gritz, Nicolas Guiard, Euan Haahr, Derek Haase, Sven-Hendrik Haase, John
Haddon, Niklas Harrysson, Daniel Heckenberg, Chris Hellmuth, Adrien Herubel,
Dan Horák, Thiago Ize, Matt Johnson, Ronan Keryell, Chris Kulla, Elvic Liang,
Max Liani, Adam Martinez, John Mertic, Bastien Montagne, Steena Monteiro,
Patrick Mours, Alexis Oblet, Erich Ocean, Mikko Ohtamaa, Jino Park, Alexei
Pawlow, Mitch Prater, Jay Reynolds, Declan Russell, Benoit Ruiz, Patrick
Scheibe, Alex Schworer, Jonathan Scruggs, Sergey Sharybin, Mark Sisson, Sandip
Shukla, Cliff Stein, Stephan Steinbach, Luya Tshimbalanga, Esteban Tovagliari,
Brecht Van Lommel, Thibault Vergne, Alexander von Knorring, Aidan Welch, Alex
Wells, Roman Zulak. (Listed alphabetically; if we've left anybody out, it is
inadvertent, please let us know.)

We cannot possibly express sufficient gratitude to the managers at Sony
Pictures Imageworks who allowed this project to proceed, supported it
wholeheartedly, and permitted us to release the source, especially Rob
Bredow, Brian Keeney, Barbara Ford, Rene Limberger, Erik Strauss, and Mike
Ford.

Huge thanks also go to the crack shading team at SPI, and the brave lookdev
TDs and CG supes willing to use OSL on their shows.  They served as our
guinea pigs, inspiration, testers, and a fantastic source of feedback. And
of course, the many engineers, TDs, and artists elsewhere who incorporated
OSL into their products and pipelines, especially the early risk-takers at
Chaos Group, Double Negative, Pixar, DNA, Isotropix, and Animal Logic. Thank
you, and we hope we've been responsive to your needs.

OSL was not developed in isolation.  We owe a debt to the individuals
and studios who patiently read early drafts of the language
specification and gave us very helpful feedback and additional ideas,
as well as to the continuing contributions and feedback of its current
developers and users at other VFX and animation studios.

The OSL implementation depends upon several other open source packages,
all with compatible licenses:

* [OpenImageIO](http://www.openimageio.org)
* [IlmBase](http://www.openexr.com)
* [LLVM Compiler Infrastructure](http://llvm.org)

OSL's documentation incorporates parts of [Markdeep](https://casual-effects.com/markdeep/)
(c) 2015-2016, Morgan McGuire, and [highlight.js](https://highlightjs.org/)
(c) 2006, Ivan Sagalaev, both distributed under BSD licenses.
