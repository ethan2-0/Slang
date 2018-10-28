class Claim:
    def __init__(self, subject, predicate_name):
        self.subject = subject
        self.predicate = type(self)
        self.predicate_name = predicate_name

    def is_equivalent_to(self, other):
        return self.subject == other.subject and self.predicate == other.predicate

    def __str__(self):
        return "{%s %s}" % (self.subject, self.predicate_name)

    def __repr__(self):
        return "Claim(subject='%s', predicate_name='%s')" % (self.subject, self.predicate_name)

class ClaimReturns(Claim):
    def __init__(self):
        Claim.__init__(self, "[method]", "returns")

class ClaimInitializes(Claim):
    # For now, this is only used in tests. Eventually, I want to enforce
    # mandatory explicit initialization, but for now I just automatically
    # initialize to null.
    def __init__(self, subject):
        Claim.__init__(self, subject, "is initialized")

class ClaimSpace:
    def __init__(self, claims=None, parent=None):
        if claims is None:
            claims = list()
        # TODO: This should check for iterability generally, and do something
        #       reasonable with non-lists.
        assert isinstance(claims, list)
        for claim in claims:
            assert isinstance(claim, Claim)
        self.claims = claims
        self.parent = parent

    def __repr__(self):
        return "ClaimSpace(%s)" % ", ".join([str(s) for s in self.claims])

    def __str__(self):
        return repr(self)

    def contains_equivalent(self, query):
        for claim in self.claims:
            if claim.is_equivalent_to(query):
                return True
        if self.parent is not None:
            return self.parent.contains_equivalent(query)
        return False

    def include_from(self, other):
        assert type(other) is ClaimSpace
        # We can't use ClaimSpace.contains_equivalent, because it could consider parents
        for claim in self.claims:
            for otherclaim in other.claims:
                if claim.is_equivalent_to(otherclaim):
                    self.claims.remove(claim)
                    break
        self.claims += other.claims

    def union(self, other):
        assert type(other) is ClaimSpace
        newspace = ClaimSpace(other.claims)
        newspace.include_from(self)
        return newspace

    def intersection(self, other):
        assert type(other) is ClaimSpace
        new_claims = []
        for claim in self.claims:
            has_counterpart = any([otherclaim.is_equivalent_to(claim) for otherclaim in other.claims])
            if has_counterpart:
                new_claims.append(claim)
        return ClaimSpace(claims=new_claims)

    def add_claims_pushy(self, *claims):
        # TODO: This will do weird things if *claims contains duplicates
        claims = list(claims)
        for claim in claims:
            assert isinstance(claim, Claim)
            for counterpart in self.claims:
                if counterpart.is_equivalent_to(claim):
                    self.claims.remove(counterpart)
        self.claims += claims

    def add_claims_conservative(self, *claims):
        claims = list(claims)
        for claim in claims:
            assert isinstance(claim, Claim)
            has_counterpart = any([selfclaim.is_equivalent_to(claim) for selfclaim in self.claims])
            if has_counterpart:
                claims.remove(claim)
        self.claims += claims

    def add_claims(self, *claims):
        return self.add_claims_conservative(*claims)
