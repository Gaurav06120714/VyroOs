# gh-pages site assets

This directory holds the static site published to
`https://gaurav06120714.github.io/VyroOs` alongside the APT repo at
`/apt/`.

The publish-apt CI workflow (`.github/workflows/publish-apt.yml`) does
two things on every successful tag build:

1. `rsync -a --delete apt-out/ pages-tree/apt/` — sync the freshly-built
   Debian archive
2. `rsync -a --delete path-a-ubuntu-remix/pages/ pages-tree/` — sync the
   site assets (index.html, style.css, favicon.svg, _config.yml)

This is more disciplined than the v0 publish step that inlined the HTML
into the workflow as a heredoc — anyone wanting to update the site now
edits these files like normal source code, and the change ships on the
next release.

## Files

- `index.html` — the landing page with hero, three-path card grid, APT
  install snippet, footer
- `style.css` — full Vyro design tokens, glassmorphism cards, gradient
  hero title, responsive at 600px breakpoint
- `favicon.svg` — same mark used in Plymouth/branding, 64×64 with rounded
  corners and accent V-stroke
- `_config.yml` — Jekyll config that just tells GitHub Pages to treat
  the tree as pre-built static content
