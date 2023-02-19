///////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006, 2007, 2008, 2009
// University of Rochester
// Department of Computer Science
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the University of Rochester nor the names of its
//      contributors may be used to endorse or promote products derived from
//      this software without specific prior written permission.
//
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

/*  edge.cpp
 *
 *  Edges encapsulate the bulk of the information about the triangulation.
 *  Each edge contains references to its endpoints and to the next
 *  edges clockwise and counterclockwise about those endpoints.
 */

#include "edge.hpp"
#include "edge_set.hpp"

// Edge may not be Delaunay.  See if it's the
// diagonal of a convex quadrilateral.  If so, check
// whether it should be flipped.  If so, return         //    c     |
// (through ref params) the edges of the                //   / \    |
// quadrilateral for future reconsideration.  In the    //  a - b   |
// private (nontransactinal) instantiation, do all of   //   \ /    |
// this only if all points I touch are closest to my    //    d     |
// seam: returns false iff called nontransactionally
// and edge cannot safely be reconsidered now.
//
template<class edgeRp, class edgeWp>
    // can be instantiated with
    // <edge::Rp, edge::Wp> or <edge::Up, edge::Up>.
bool reconsider(edgeWp self, const int seam,
                edge::Sp& e1, edge::Sp& e2, edge::Sp& e3, edge::Sp& e4) {
    e1 = edge::Sp(NULL);  e2 = edge::Sp(NULL);
    e3 = edge::Sp(NULL);  e4 = edge::Sp(NULL);
    self->set_tentative(false, self);
    point* a = self->get_points(0, self);
    assert (txnal<edgeRp>() || closest_seam(a) == seam);
    point* b = self->get_points(1, self);
    assert (txnal<edgeRp>() || closest_seam(b) == seam);
    edgeRp ac(self->get_neighbors(0, ccw, self));
    edgeRp bc(self->get_neighbors(1, cw, self));
    point* c = ac->get_points(1-index_of(ac, a), ac);
    // a and b are assumed to be closest to my seam.
    // I have to check c and d.
    if (!txnal<edgeRp>() && closest_seam(c) != seam) {
        // I can't safely consider this flip in this phase of
        // the algorithm.  Defer to synchronized phase.
        self->set_tentative(true, self);
        return false;
    }
    if (c != bc->get_points(1-index_of(bc, b), bc)) {
        // No triangle on the c side; we're an external edge
        return true;
    }
    edgeRp ad(self->get_neighbors(0, cw, self));
    edgeRp bd(self->get_neighbors(1, ccw, self));
    point* d = ad->get_points(1-index_of(ad, a), ad);
    if (!txnal<edgeRp>() && closest_seam(d) != seam) {
        // I can't safely consider this flip in this phase of
        // the algorithm.  Defer to synchronized phase.
        self->set_tentative(true, self);
        return false;
    }
    if (d != bd->get_points(1-index_of(bd, b), bd)) {
        // No triangle on the d side; we're an external edge
        return true;
    }
    if (encircled(b, c, a, d, ccw) || encircled(a, d, b, c, ccw)) {
        // other diagonal is locally Delaunay; we're not
        destroy(self);      // can't wait for delayed destructor
        edgeWp ac_w(ac);  edgeWp ad_w(ad);
        edgeWp bc_w(bc);  edgeWp bd_w(bd);
        edge::Sp dum;
        edge::create(c, d, bc_w, bd_w, cw, dum);
            // Aliasing problem here: since edge constructor modifies
            // neighbors, I can't safely use ac, bc, ad, or bd after this
            // call.  Must use writable versions instead.
        if (!ac_w->get_tentative(ac_w)) {
            ac_w->set_tentative(true, ac_w);  e1 = ac_w;
        }
        if (!ad_w->get_tentative(ad_w)) {
            ad_w->set_tentative(true, ad_w);  e2 = ad_w;
        }
        if (!bc_w->get_tentative(bc_w)) {
            bc_w->set_tentative(true, bc_w);  e3 = bc_w;
        }
        if (!bd_w->get_tentative(bd_w)) {
            bd_w->set_tentative(true, bd_w);  e4 = bd_w;
        }
    }
    return true;
}
// explicit instantiations:
template bool reconsider<edge::Up, edge::Up>
    (edge::Up, const int, edge::Sp&, edge::Sp&, edge::Sp&, edge::Sp&);
template bool reconsider<edge::Rp, edge::Wp>
    (edge::Wp, const int, edge::Sp&, edge::Sp&, edge::Sp&, edge::Sp&);

#ifdef FGL
// Alternative, FGL version: optimistic fine-grain locking.
// Identifies the relevant edges and points, then locks
// them in canonical order, then double-checks to make
// sure nothing has changed.
//
void synchronized_reconsider(edge::Up self, const int seam,
                             simple_queue<edge::Sp> *tentative_edges) {
    self->set_tentative(false, self);
        // Do this first, to avoid window where I turn it off
        // after somebody else has turned it on.
    point* a = self->get_points(0, self);
    point* b = self->get_points(1, self);
    edge::Up ac(self->get_neighbors(0, ccw, self));
    edge::Up bc(self->get_neighbors(1, cw, self));
    int aci = index_of(ac, a);
    int bci = index_of(bc, b);
    if (aci == -1 || bci == -1) {
        // inconsistent; somebody has already been here
        return;
    }
    point* c = ac->get_points(1-aci, ac);
    if (c != bc->get_points(1-bci, bc)) {
        // No triangle on the c side; we're an external edge
        return;
    }
    edge::Up ad(self->get_neighbors(0, cw, self));
    edge::Up bd(self->get_neighbors(1, ccw, self));
    int adi = index_of(ad, a);
    int bdi = index_of(bd, b);
    if (adi == -1 || bdi == -1) {
        // inconsistent; somebody has already been here
        return;
    }
    point* d = ad->get_points(1-adi, ad);
    if (d != bd->get_points(1-bdi, bd)) {
        // No triangle on the d side; we're an external edge
        return;
    }
    {
        point_set S;
        with_locked_points cs(S | a | b | c | d);

        if (!(edges->contains(self)
                && edges->contains(ac) && edges->contains(bc)
                && edges->contains(ad) && edges->contains(bd))) {
            // inconsistent; somebody has already been here
            return;
        }
        if (!(ac == self->get_neighbors(0, ccw, self)
                && bc == self->get_neighbors(1, cw, self)
                && ad == self->get_neighbors(0, cw, self)
                && bd == self->get_neighbors(1, ccw, self))) {
            // inconsistent; somebody has already been here
            return;
        }
        if (!(aci == index_of(ac, a)
                && bci == index_of(bc, b)
                && adi == index_of(ad, a)
                && bdi == index_of(bd, b))) {
            // inconsistent; somebody has already been here
            return;
        }
        if (!(c == ac->get_points(1-aci, ac)
                && c == bc->get_points(1-bci, bc)
                && d == ad->get_points(1-adi, ad)
                && d == bd->get_points(1-bdi, bd))) {
            // inconsistent; somebody has already been here
            return;
        }
        if (encircled(b, c, a, d, ccw) || encircled(a, d, b, c, ccw)) {
            // other diagonal is locally Delaunay; we're not
            destroy(self);      // can't wait for delayed destructor
            edge::Sp dum;
            edge::create(c, d, bc, bd, cw, dum);
            ac->set_tentative(true, ac);  tentative_edges->enqueue(ac, seam);
            ad->set_tentative(true, ad);  tentative_edges->enqueue(ad, seam);
            bc->set_tentative(true, bc);  tentative_edges->enqueue(bc, seam);
            bd->set_tentative(true, bd);  tentative_edges->enqueue(bd, seam);
        }
    }
}
#endif  // FGL

// Utility routine for constructor.
// can be instantiated with <edge::Wp> or with <edge::Up>
//
template <template <class> class wrtp>
void edge::initialize_end(point* p, wrtp<edge> e, int end, int dir,
                          wrtp<edge>& self_w) {
    if (e == 0) {
        self_w->set_neighbors(end, dir, self_w, self_w);
        self_w->set_neighbors(end, 1-dir, self_w, self_w);
        p->set_first_edge<wrtp>(self_w);
    } else {
        int i = index_of(e, p);
        self_w->set_neighbors(end, 1-dir, e, self_w);
        self_w->set_neighbors(end, dir, e->get_neighbors(i, dir, e), self_w);
        e->set_neighbors(i, dir, self_w, e);

        // accessing neighbors directly in constructor code. This must no be a
        // shared edge during this call
        edge::Sp nbor_s(self_w->get_neighbors(end, dir, self_w));
        wrtp<edge> nbor_w(nbor_s);
        i = index_of(nbor_w, p);
        nbor_w->set_neighbors(i, 1-dir, self_w, nbor_w);
    }
}

// Edge "constructor": connect points A and B, inserting dir (CW or CCW)
// of edge Ea at the A end and 1-dir of edge Eb at the B end.
// Either or both of Ea and Eb may be null.
// Sets reference parameter self to be edge::Sp(new edge).
// Can be instantiated with <edge::Wp> or with <edge::Up>
//
template <template <class> class wrtp>
void edge::create(point* a, point* b, wrtp<edge> ea, wrtp<edge> eb, int dir,
                  sh_ptr<edge>& self)
{
    self = edge::Sp(new edge());
    wrtp<edge> self_w = wrtp<edge>(self);
    self_w->set_tentative(false, self_w);
    self_w->set_points(0, a, self_w);
    self_w->set_points(1, b, self_w);
    initialize_end(a, ea, 0, dir, self_w);
    initialize_end(b, eb, 1, 1-dir, self_w);
    typename wrtp<edge>::rd self_r(self);
    edges->insert(self_r);
}

// Edge destructor: take self out of edges, point edge lists.
// Should only be called when flipping an edge, so destroyed
// edge should have neighbors at both ends.
// Caller should hold locks that cover endpoints and neighbors.
//
template <template <class> class wrtp>
void destroy(wrtp<edge>& e) {
    edges->erase(e);
    for (int i = 0; i < 2; i++) {
        point* p = e->get_points(i, e);
        wrtp<edge> cw_nbor(e->get_neighbors(i, cw, e));
        wrtp<edge> ccw_nbor(e->get_neighbors(i, ccw, e));
        int cw_index = index_of(cw_nbor, p);
        int ccw_index = index_of(ccw_nbor, p);
        cw_nbor->set_neighbors(cw_index, ccw, ccw_nbor, cw_nbor);
        ccw_nbor->set_neighbors(ccw_index, cw, cw_nbor, ccw_nbor);
        if (p->get_first_edge<wrtp>() == e)
            p->set_first_edge<wrtp>(ccw_nbor);
    }
    tx_delete(e);
}
// explicit instantiations:
template void destroy<un_ptr>(un_ptr<edge>& e);
template void destroy<wr_ptr>(wr_ptr<edge>& e);
