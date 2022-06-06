/**
 * Creating a sidebar enables you to:
 - create an ordered group of docs
 - render a sidebar for each doc of that group
 - provide next/previous navigation

 The sidebars can be generated from the filesystem, or explicitly defined here.

 Create as many sidebars as you want.
 */

// @ts-check

/** @type {import('@docusaurus/plugin-content-docs').SidebarsConfig} */
const sidebars = {
  // By default, Docusaurus generates a sidebar from the docs folder structure
  apiSidebar: [
    {
      type: 'autogenerated',
      dirName: 'Api',
    },
  ],
  tutorialSidebar: [
    {
      type: "doc",
      id: "Introduction",
      label: "Introduction",
    },
    {
      type: "category",
      label: "Guides",
      items: [
        {
          type: "autogenerated",
          dirName: "Guides",
        },
      ],
    },
    {
      type: "category",
      label: "Discussions",
      items: [
        {
          type: "autogenerated",
          dirName: "Discussions",
        },
      ],
    },
    {
      type: "category",
      label: "Internal",
      items: [
        {
          type: "autogenerated",
          dirName: "Internal",
        },
      ],
    },
  ],

  // But you can create a sidebar manually
  /*
  tutorialSidebar: [
    {
      type: 'category',
      label: 'Tutorial',
      items: ['hello'],
    },
  ],
   */
};

module.exports = sidebars;
