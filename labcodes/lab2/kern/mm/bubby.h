#ifndef BUBBY_S
#define BUBBY_S
unsigned size[200000],maxx[200000];
bool IS_POWER_2(unsigned x)
{
	while(x)
	{
		if (x&1)
			return x==1;
		x>>=1;
	}
	return 0;
}
unsigned max(unsigned x,unsigned y)
{
	if(x<y)
		return y;
	return x;
}
unsigned MAX_POWER_2(unsigned x)
{
	if(x==0)
		return 0;
	unsigned w = 1;
	while(w<=x)
		w*=2;
	return w/2;
}
void build(unsigned now,unsigned l,unsigned r)
{
	assert(now!=0);
	if(l==r)
	{
		maxx[now]=1;
		return;
	}
	unsigned mid;
	mid=l+MAX_POWER_2(r-l)-1;
	build(now*2,l,mid);
	build(now*2+1,mid+1,r);
	if (r-mid == mid-l+1)
		size[now]=maxx[now]=maxx[now*2]+maxx[now*2+1];
	else
		size[now]=maxx[now]=maxx[now*2];
	//cprintf("%u\n",x->size);
	//cprintf("%u\t%u\t%u\t%u\n",now,l,r,x->maxx[now]);
}
int find(unsigned now,unsigned l,unsigned r,unsigned cal)
{
	//cprintf("%u\t%u\t%u\t%u\t%u\n",now,l,r,cal,maxx[now]);
	if (l>r)
		return -1;
	if (maxx[now]<cal)
		return -1;
	if (maxx[now]==cal&&size[now]==cal)
	{
		maxx[now]=0;
		return l;
	}
	unsigned mid,w;
	mid=l+MAX_POWER_2(r-l)-1;
	w = find(now*2,l,mid,cal);
	if (w==-1)
	{
		w = find(now*2+1,mid+1,r,cal);
	}
	maxx[now] = max(maxx[now*2],maxx[now*2+1]);
	return w;
}
void clr(unsigned now,unsigned l,unsigned r,unsigned h,unsigned t)
{
	//cprintf("%u\t%u\t%u\t%u\t%u\n",now,l,r,h,t);
	if(now>100000)
		return;
	if (l==h&&r==t)
	{
		maxx[now]=r-l+1;
		return;
	}
	unsigned mid;
	mid=l+MAX_POWER_2(r-l)-1;
	if (t<=mid)
		clr(now*2,l,mid,h,t);
	else
		clr(now*2+1,mid+1,r,h,t);
	if (maxx[now*2]+maxx[now*2+1]==r-l+1)
		maxx[now]=r-l+1;
	else
		maxx[now]=max(maxx[now*2],maxx[now*2+1]);
}
void bubby_new(unsigned size)
{
	if(size==0)
		return;
	build(1,1,size);
}
#endif
