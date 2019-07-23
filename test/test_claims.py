import sys
import os.path

initial_sys_path = sys.path
sys.path = [os.path.join(os.path.dirname(__file__), "../compiler")] + sys.path
import claims
sys.path = initial_sys_path

class TestClaim:
    def test_claim_equivalence(self):
        class ClaimSubclass1(claims.Claim):
            def __init__(self, subject):
                claims.Claim.__init__(self, subject, "subclass1")

        class ClaimSubclass2(claims.Claim):
            def __init__(self, subject):
                claims.Claim.__init__(self, subject, "subclass2")

        claim11 = ClaimSubclass1("subject1")
        claim11_2 = ClaimSubclass1("subject1")
        claim12 = ClaimSubclass1("subject2")
        claim21 = ClaimSubclass2("subject1")
        claim22 = ClaimSubclass2("subject2")

        assert claim11.is_equivalent_to(claim11)
        assert claim11.is_equivalent_to(claim11_2)
        assert claim11_2.is_equivalent_to(claim11)

        assert not claim11.is_equivalent_to(claim12)
        assert not claim11.is_equivalent_to(claim21)
        assert not claim11.is_equivalent_to(claim22)

        assert not claim12.is_equivalent_to(claim11)
        assert not claim21.is_equivalent_to(claim11)
        assert not claim22.is_equivalent_to(claim11)

    def test_claim_returns(self):
        claim1 = claims.ClaimReturns()
        claim2 = claims.ClaimReturns()

        assert claim1.is_equivalent_to(claim2)

    def test_claim_initializes(self):
        claim1 = claims.ClaimInitializes("1")
        claim1_2 = claims.ClaimInitializes("1")
        claim2 = claims.ClaimInitializes("2")

        assert claim1.is_equivalent_to(claim1_2)
        assert claim1_2.is_equivalent_to(claim1)
        assert not claim1.is_equivalent_to(claim2)

class TestClaimSpace:
    def test_add_claims(self):
        claim = claims.ClaimReturns()
        claim2 = claims.ClaimReturns()
        space = claims.ClaimSpace()
        space.add_claims_conservative(claim)
        assert space.claims == [claim]
        space.add_claims_conservative(claim2)
        assert space.claims == [claim] and space.claims[0] is claim
        space.add_claims_pushy(claim2)
        assert space.claims == [claim2] and space.claims[0] is claim2

        space2 = claims.ClaimSpace()
        init_claim = claims.ClaimInitializes("a")
        space2.add_claims_conservative(init_claim)
        space2.add_claims_conservative(claims.ClaimInitializes("a"))
        assert space2.claims == [init_claim] and space2.claims[0] is init_claim
        init_claim2 = claims.ClaimInitializes("b")
        space2.add_claims_conservative(init_claim2)
        assert space2.claims == [init_claim, init_claim2] and space2.claims[0] is init_claim and space2.claims[1] is init_claim2

    def test_union(self):
        claim1 = claims.ClaimInitializes("a")
        claim2 = claims.ClaimInitializes("b")
        space1 = claims.ClaimSpace([claim1])
        space2 = claims.ClaimSpace([claim2])
        assert set(space1.union(space2).claims) == {claim1, claim2}
        returns = claims.ClaimReturns()
        space2.add_claims_conservative(returns)
        assert set(space1.union(space2).claims) == {claim1, claim2, returns}

    def test_intersection(self):
        claim1 = claims.ClaimInitializes("a")
        claim2 = claims.ClaimInitializes("b")
        space1 = claims.ClaimSpace([claim1, claim2])
        space2 = claims.ClaimSpace([claim1])
        assert set(space1.intersection(space2).claims) == {claim1}
        space2.add_claims_conservative(claim2)
        assert set(space1.intersection(space2).claims) == {claim1, claim2}

    def test_include_from(self):
        claim1 = claims.ClaimInitializes("a")
        claim2 = claims.ClaimInitializes("b")
        space1 = claims.ClaimSpace([claim1])
        space2 = claims.ClaimSpace([claim2])
        space1.include_from(space2)
        assert set(space1.claims) == {claim1, claim2}
        assert set(space2.claims) == {claim2}

    def test_contains_equivalent(self):
        claim1 = claims.ClaimInitializes("a")
        space = claims.ClaimSpace([claim1])
        assert space.contains_equivalent(claims.ClaimInitializes("a"))
        assert not space.contains_equivalent(claims.ClaimInitializes("b"))
        space.add_claims_conservative(claims.ClaimInitializes("b"))
        assert space.contains_equivalent(claims.ClaimInitializes("b"))

    def test_contains_equivalent_parent(self):
        claim1 = claims.ClaimInitializes("a")
        parent = claims.ClaimSpace()
        child = claims.ClaimSpace(parent=parent)
        assert not child.contains_equivalent(claim1)
        parent.add_claims_conservative(claim1)
        assert child.contains_equivalent(claim1)
