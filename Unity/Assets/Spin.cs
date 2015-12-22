using UnityEngine;
using System.Collections;

public class Spin : MonoBehaviour {

	[Range(0,10)]
	public float RotateX = 0.1f;
	[Range(0,10)]
	public float RotateY = 1.0f;
	[Range(0,10)]
	public float RotateZ = 0.0f;

	// Use this for initialization
	void Start () {
	
	}
	
	// Update is called once per frame
	void Update () {
		transform.Rotate (new Vector3 (RotateX, RotateY, RotateZ));
	}
}
