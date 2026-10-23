/*
 * dc-lite.js: a small standalone renderer for the design canvas screen format.
 * Supports {{path}} holes, <sc-for>, <sc-if>, onClick handlers, data-props defaults
 * and setState re-rendering, which is everything the Snake screens use.
 */
(function () {
  'use strict';

  function DCLogic(props) {
    this.props = props || {};
    this.state = null;
  }
  DCLogic.prototype.setState = function (patch) {
    var next = typeof patch === 'function' ? patch(this.state || {}, this.props) : patch;
    this.state = Object.assign({}, this.state || {}, next);
    render();
  };
  DCLogic.prototype.forceUpdate = function () { render(); };
  window.DCLogic = DCLogic;

  var HOLE = /\{\{\s*([^}]+?)\s*\}\}/g;
  var WHOLE = /^\s*\{\{\s*([^}]+?)\s*\}\}\s*$/;
  var SVG_NS = 'http://www.w3.org/2000/svg';
  var comp = null;
  var tpl = null;
  var root = null;

  function lookup(path, scope) {
    path = path.trim();
    if (path === 'true') return true;
    if (path === 'false') return false;
    if (/^-?\d+(\.\d+)?$/.test(path)) return Number(path);
    var parts = path.split('.');
    var v = scope;
    for (var i = 0; i < parts.length; i++) {
      if (v === null || v === undefined) return undefined;
      v = v[parts[i]];
    }
    return v;
  }

  function fill(str, scope) {
    return str.replace(HOLE, function (_, p) {
      var v = lookup(p, scope);
      return v === undefined || v === null ? '' : String(v);
    });
  }

  function renderChildren(src, scope, out, inSvg) {
    for (var c = src.firstChild; c; c = c.nextSibling) renderNode(c, scope, out, inSvg);
  }

  function renderNode(node, scope, out, inSvg) {
    if (node.nodeType === 3) {
      out.appendChild(document.createTextNode(fill(node.nodeValue, scope)));
      return;
    }
    if (node.nodeType !== 1) return;
    var tag = node.localName;

    if (tag === 'sc-for') {
      var list = lookup((node.getAttribute('list') || '').replace(/[{}]/g, ''), scope) || [];
      var as = node.getAttribute('as') || 'item';
      for (var i = 0; i < list.length; i++) {
        var child = Object.create(scope);
        child[as] = list[i];
        child.$index = i;
        renderChildren(node, child, out, inSvg);
      }
      return;
    }
    if (tag === 'sc-if') {
      if (lookup((node.getAttribute('value') || '').replace(/[{}]/g, ''), scope)) {
        renderChildren(node, scope, out, inSvg);
      }
      return;
    }
    if (tag === 'helmet') return;

    var svg = inSvg || tag === 'svg';
    var el = svg ? document.createElementNS(SVG_NS, node.tagName.toLowerCase() === 'svg' ? 'svg' : node.tagName) : document.createElement(tag);

    for (var a = 0; a < node.attributes.length; a++) {
      var attr = node.attributes[a];
      var name = attr.name;
      if (name.indexOf('hint-') === 0) continue;
      var whole = attr.value.match(WHOLE);
      if (name.indexOf('on') === 0 && whole) {
        var fn = lookup(whole[1], scope);
        if (typeof fn === 'function') el.addEventListener(name.slice(2).toLowerCase(), fn);
        continue;
      }
      var val = whole ? lookup(whole[1], scope) : fill(attr.value, scope);
      if (val === false || val === undefined || val === null) continue;
      el.setAttribute(name, val === true ? '' : String(val));
    }
    renderChildren(node, scope, el, svg);
    out.appendChild(el);
  }

  function render() {
    var vals = comp.renderVals() || {};
    var frag = document.createDocumentFragment();
    renderChildren(tpl.content, vals, frag, false);
    root.innerHTML = '';
    root.appendChild(frag);
  }

  window.dcBoot = function (Component, propsSpec) {
    var props = {};
    Object.keys(propsSpec || {}).forEach(function (k) {
      if (k.charAt(0) !== '$' && propsSpec[k] && 'default' in propsSpec[k]) props[k] = propsSpec[k].default;
    });
    tpl = document.getElementById('dc-template');
    root = document.getElementById('dc-root');
    comp = new Component(props);
    render();
  };
})();
